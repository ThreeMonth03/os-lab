#include "kernel/display/window_repaint_planner.hpp"

namespace kernel::display
{

namespace
{

uint64_t rect_area(Rect rect)
{
    return rect.width * rect.height;
}

bool same_rect(Rect lhs, Rect rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width &&
           lhs.height == rhs.height;
}

bool same_size(Rect lhs, Rect rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

uint64_t rect_right(Rect rect)
{
    return rect.x + rect.width;
}

uint64_t rect_bottom(Rect rect)
{
    return rect.y + rect.height;
}

void append_rect_minus(WindowRepaintRegionList & regions, Rect rect, Rect cutout)
{
    const Rect overlap = intersect_rect(rect, cutout);
    if (overlap.empty())
    {
        regions.append(rect);
        return;
    }

    if (overlap.y > rect.y)
    {
        regions.append({rect.x, rect.y, rect.width, overlap.y - rect.y});
    }
    if (rect_bottom(overlap) < rect_bottom(rect))
    {
        regions.append({
            rect.x,
            rect_bottom(overlap),
            rect.width,
            rect_bottom(rect) - rect_bottom(overlap),
        });
    }
    if (overlap.x > rect.x)
    {
        regions.append({rect.x, overlap.y, overlap.x - rect.x, overlap.height});
    }
    if (rect_right(overlap) < rect_right(rect))
    {
        regions.append({
            rect_right(overlap),
            overlap.y,
            rect_right(rect) - rect_right(overlap),
            overlap.height,
        });
    }
}

bool visible_entry(const WindowStackEntry * entry)
{
    return entry != nullptr && entry->visible();
}

size_t stack_index_of(const WindowStack & stack, WindowSessionId id)
{
    for (size_t index = 0; index < stack.size(); ++index)
    {
        const WindowStackEntry * entry = stack.at(index);
        if (entry != nullptr && entry->id == id)
        {
            return index;
        }
    }
    return stack.size();
}

} // namespace

void WindowRepaintRegionList::reset(Rect bounds)
{
    bounds_ = bounds;
    clear();
}

void WindowRepaintRegionList::clear()
{
    for (auto & region : regions_)
    {
        region = {};
    }
    count_ = 0;
    full_screen_fallback_ = false;
    total_area_ = 0;
    largest_area_ = 0;
}

WindowRepaintAppendResult WindowRepaintRegionList::append(Rect rect)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty())
    {
        return WindowRepaintAppendResult::Ignored;
    }

    if (full_screen_fallback_)
    {
        return WindowRepaintAppendResult::FullscreenFallback;
    }

    if (count_ >= kMaxWindowRepaintRegions)
    {
        return fallback_to_fullscreen();
    }

    regions_[count_++] = rect;
    rebuild_stats();
    return WindowRepaintAppendResult::Queued;
}

Rect WindowRepaintRegionList::at(size_t index) const
{
    if (index >= count_)
    {
        return {};
    }
    return regions_[index];
}

WindowRepaintAppendResult WindowRepaintRegionList::fallback_to_fullscreen()
{
    clear();
    full_screen_fallback_ = true;
    if (!bounds_.empty())
    {
        regions_[0] = bounds_;
        count_ = 1;
    }
    rebuild_stats();
    return WindowRepaintAppendResult::FullscreenFallback;
}

void WindowRepaintRegionList::rebuild_stats()
{
    total_area_ = 0;
    largest_area_ = 0;
    for (size_t index = 0; index < count_; ++index)
    {
        const uint64_t area = rect_area(regions_[index]);
        total_area_ += area;
        if (area > largest_area_)
        {
            largest_area_ = area;
        }
    }
}

WindowRepaintPlanner::WindowRepaintPlanner(Rect desktop_bounds,
                                           WindowFrameConfig frame_config)
    : desktop_bounds_(desktop_bounds)
    , frame_config_(frame_config)
{
}

WindowRepaintRegionList WindowRepaintPlanner::move_damage(Rect previous_bounds,
                                                          Rect current_bounds) const
{
    WindowRepaintRegionList regions(desktop_bounds_);
    if (previous_bounds.empty() && current_bounds.empty())
    {
        return regions;
    }
    if (same_rect(previous_bounds, current_bounds))
    {
        regions.append(current_bounds);
        return regions;
    }

    regions.append(current_bounds);
    append_rect_minus(regions, previous_bounds, current_bounds);
    return regions;
}

WindowRepaintRegionList WindowRepaintPlanner::mutation_damage(
    WindowSessionMutation mutation) const
{
    WindowRepaintRegionList regions(desktop_bounds_);
    if (!mutation.valid())
    {
        return regions;
    }

    const bool previous_visible = mutation.previous.visible() && !mutation.previous.closed();
    const bool current_visible = mutation.current.visible() && !mutation.current.closed();
    if (previous_visible && current_visible &&
        !same_rect(mutation.previous.bounds.outer, mutation.current.bounds.outer) &&
        same_size(mutation.previous.bounds.outer, mutation.current.bounds.outer))
    {
        return move_damage(mutation.previous.bounds.outer, mutation.current.bounds.outer);
    }

    if (previous_visible != current_visible ||
        !same_rect(mutation.previous.bounds.outer, mutation.current.bounds.outer))
    {
        regions.append(mutation.repaint_bounds);
    }
    return regions;
}

WindowRepaintRegionList WindowRepaintPlanner::visual_state_damage(Rect outer_bounds) const
{
    WindowRepaintRegionList regions(desktop_bounds_);
    append_chrome_damage(regions, outer_bounds);
    return regions;
}

WindowRepaintRegionList WindowRepaintPlanner::stack_transition_damage(
    const WindowStack & previous,
    const WindowStack & current,
    const WindowSessionRegistry & sessions) const
{
    WindowRepaintRegionList regions(desktop_bounds_);

    for (size_t current_index = 0; current_index < current.size(); ++current_index)
    {
        const WindowStackEntry * current_entry = current.at(current_index);
        if (!visible_entry(current_entry))
        {
            continue;
        }

        const size_t previous_index = stack_index_of(previous, current_entry->id);
        if (previous_index >= previous.size() || current_index <= previous_index)
        {
            continue;
        }

        const WindowSession * raised = sessions.find(current_entry->id);
        if (raised == nullptr || !raised->visible() || raised->closed())
        {
            continue;
        }

        for (size_t covered_index = previous_index + 1; covered_index <= current_index;
             ++covered_index)
        {
            const WindowStackEntry * covered_entry = previous.at(covered_index);
            if (!visible_entry(covered_entry))
            {
                continue;
            }

            const WindowSession * covered = sessions.find(covered_entry->id);
            if (covered == nullptr || !covered->visible() || covered->closed())
            {
                continue;
            }
            regions.append(intersect_rect(raised->bounds.outer, covered->bounds.outer));
        }
    }

    return regions;
}

void WindowRepaintPlanner::append_chrome_damage(WindowRepaintRegionList & regions,
                                                Rect outer_bounds) const
{
    if (outer_bounds.empty())
    {
        return;
    }

    const WindowFrameMetrics metrics = WindowChrome::metrics_for(outer_bounds, frame_config_);
    if (!metrics.visible || !metrics.valid() || metrics.border_thickness == 0)
    {
        regions.append(outer_bounds);
        return;
    }

    const uint64_t border = metrics.border_thickness;
    regions.append(metrics.title_bar_bounds);
    regions.append({
        outer_bounds.x,
        metrics.title_bar_bounds.y + metrics.title_bar_bounds.height,
        border,
        outer_bounds.height - metrics.title_bar_bounds.height,
    });
    regions.append({
        outer_bounds.x + outer_bounds.width - border,
        metrics.title_bar_bounds.y + metrics.title_bar_bounds.height,
        border,
        outer_bounds.height - metrics.title_bar_bounds.height,
    });
    regions.append({
        outer_bounds.x,
        outer_bounds.y + outer_bounds.height - border,
        outer_bounds.width,
        border,
    });
}

} // namespace kernel::display
