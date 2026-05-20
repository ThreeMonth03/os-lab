#include "kernel/display/window_interaction.hpp"

#include "kernel/display/window_preview.hpp"
#include "kernel/display/window_repaint_planner.hpp"

namespace
{

namespace display = kernel::display;

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

uint64_t add_delta_clamped(uint64_t value, uint64_t anchor, uint64_t pointer)
{
    if (pointer >= anchor)
    {
        return value + (pointer - anchor);
    }

    const uint64_t delta = anchor - pointer;
    return delta >= value ? 0 : value - delta;
}

uint64_t clamp_u64(uint64_t value, uint64_t minimum, uint64_t maximum)
{
    if (maximum < minimum)
    {
        return minimum;
    }
    return min_u64(max_u64(value, minimum), maximum);
}

uint64_t rect_right(display::Rect rect)
{
    return rect.x + rect.width;
}

uint64_t rect_bottom(display::Rect rect)
{
    return rect.y + rect.height;
}

uint64_t outer_width_for_client(display::WindowResizeConstraints constraints)
{
    if (!constraints.frame_config.visible)
    {
        return constraints.minimum_client_width;
    }

    return constraints.minimum_client_width + (constraints.frame_config.border_thickness * 2);
}

uint64_t outer_height_for_client(display::WindowResizeConstraints constraints)
{
    if (!constraints.frame_config.visible)
    {
        return constraints.minimum_client_height;
    }

    return constraints.minimum_client_height + constraints.frame_config.title_bar_height +
           constraints.frame_config.border_thickness;
}

bool hit_region_is_resize(display::WindowChromeHitRegion region)
{
    return display::resize_edges_for_hit_region(region) != display::WindowResizeEdge::None;
}

void record_repaint_regions(display::WindowInteractionReplayStats & stats,
                            const display::WindowRepaintRegionList & regions,
                            bool commit)
{
    if (regions.full_screen_fallback())
    {
        ++stats.full_screen_fallback_count;
    }
    for (size_t index = 0; index < regions.count(); ++index)
    {
        const display::Rect rect = regions.at(index);
        const uint64_t area = rect.width * rect.height;
        if (commit)
        {
            stats.commit_repaint_pixels += area;
        }
        else
        {
            stats.preview_repaint_pixels += area;
        }
        if (area > stats.largest_repaint_area)
        {
            stats.largest_repaint_area = area;
        }
    }
}

} // namespace

namespace kernel::display
{

bool resize_edge_contains(WindowResizeEdge edges, WindowResizeEdge edge)
{
    return (static_cast<uint8_t>(edges) & static_cast<uint8_t>(edge)) != 0;
}

WindowResizeEdge resize_edges_for_hit_region(WindowChromeHitRegion region)
{
    switch (region)
    {
    case WindowChromeHitRegion::ResizeLeft:
        return WindowResizeEdge::Left;
    case WindowChromeHitRegion::ResizeRight:
        return WindowResizeEdge::Right;
    case WindowChromeHitRegion::ResizeTop:
        return WindowResizeEdge::Top;
    case WindowChromeHitRegion::ResizeBottom:
        return WindowResizeEdge::Bottom;
    case WindowChromeHitRegion::ResizeTopLeft:
        return static_cast<WindowResizeEdge>(static_cast<uint8_t>(WindowResizeEdge::Top) |
                                             static_cast<uint8_t>(WindowResizeEdge::Left));
    case WindowChromeHitRegion::ResizeTopRight:
        return static_cast<WindowResizeEdge>(static_cast<uint8_t>(WindowResizeEdge::Top) |
                                             static_cast<uint8_t>(WindowResizeEdge::Right));
    case WindowChromeHitRegion::ResizeBottomLeft:
        return static_cast<WindowResizeEdge>(static_cast<uint8_t>(WindowResizeEdge::Bottom) |
                                             static_cast<uint8_t>(WindowResizeEdge::Left));
    case WindowChromeHitRegion::ResizeBottomRight:
        return static_cast<WindowResizeEdge>(static_cast<uint8_t>(WindowResizeEdge::Bottom) |
                                             static_cast<uint8_t>(WindowResizeEdge::Right));
    case WindowChromeHitRegion::None:
    case WindowChromeHitRegion::Border:
    case WindowChromeHitRegion::TitleBar:
    case WindowChromeHitRegion::Content:
    case WindowChromeHitRegion::CloseButton:
        return WindowResizeEdge::None;
    }
    return WindowResizeEdge::None;
}

PointerCursorShape cursor_shape_for_hit_region(WindowChromeHitRegion region)
{
    switch (region)
    {
    case WindowChromeHitRegion::TitleBar:
        return PointerCursorShape::Move;
    case WindowChromeHitRegion::ResizeLeft:
    case WindowChromeHitRegion::ResizeRight:
        return PointerCursorShape::ResizeHorizontal;
    case WindowChromeHitRegion::ResizeTop:
    case WindowChromeHitRegion::ResizeBottom:
        return PointerCursorShape::ResizeVertical;
    case WindowChromeHitRegion::ResizeTopRight:
    case WindowChromeHitRegion::ResizeBottomLeft:
        return PointerCursorShape::ResizeDiagonalForward;
    case WindowChromeHitRegion::ResizeTopLeft:
    case WindowChromeHitRegion::ResizeBottomRight:
        return PointerCursorShape::ResizeDiagonalBackward;
    case WindowChromeHitRegion::None:
    case WindowChromeHitRegion::Border:
    case WindowChromeHitRegion::Content:
    case WindowChromeHitRegion::CloseButton:
        return PointerCursorShape::Arrow;
    }
    return PointerCursorShape::Arrow;
}

WindowMoveDrag WindowMoveDrag::begin(Rect start_bounds,
                                     uint64_t anchor_x,
                                     uint64_t anchor_y,
                                     Rect work_area)
{
    return {start_bounds, anchor_x, anchor_y, work_area};
}

WindowMoveDrag::WindowMoveDrag(Rect start_bounds,
                               uint64_t anchor_x,
                               uint64_t anchor_y,
                               Rect work_area)
    : start_bounds_(start_bounds)
    , anchor_x_(anchor_x)
    , anchor_y_(anchor_y)
    , work_area_(work_area)
{
}

bool WindowMoveDrag::valid() const
{
    return !start_bounds_.empty() && !work_area_.empty();
}

Rect WindowMoveDrag::bounds_for(uint64_t pointer_x, uint64_t pointer_y) const
{
    if (!valid())
    {
        return {};
    }

    const uint64_t work_right = rect_right(work_area_);
    const uint64_t work_bottom = rect_bottom(work_area_);
    if (start_bounds_.width > work_area_.width || start_bounds_.height > work_area_.height)
    {
        return {};
    }

    const uint64_t requested_x = add_delta_clamped(start_bounds_.x, anchor_x_, pointer_x);
    const uint64_t requested_y = add_delta_clamped(start_bounds_.y, anchor_y_, pointer_y);
    const uint64_t max_x = work_right - start_bounds_.width;
    const uint64_t max_y = work_bottom - start_bounds_.height;

    return {
        clamp_u64(requested_x, work_area_.x, max_x),
        clamp_u64(requested_y, work_area_.y, max_y),
        start_bounds_.width,
        start_bounds_.height,
    };
}

WindowResizeDrag WindowResizeDrag::begin(Rect start_bounds,
                                         uint64_t anchor_x,
                                         uint64_t anchor_y,
                                         WindowResizeEdge edges,
                                         WindowResizeConstraints constraints)
{
    return {start_bounds, anchor_x, anchor_y, edges, constraints};
}

WindowResizeDrag::WindowResizeDrag(Rect start_bounds,
                                   uint64_t anchor_x,
                                   uint64_t anchor_y,
                                   WindowResizeEdge edges,
                                   WindowResizeConstraints constraints)
    : start_bounds_(start_bounds)
    , anchor_x_(anchor_x)
    , anchor_y_(anchor_y)
    , edges_(edges)
    , constraints_(constraints)
{
}

bool WindowResizeDrag::valid() const
{
    return !start_bounds_.empty() && !constraints_.work_area.empty() &&
           constraints_.minimum_client_width > 0 && constraints_.minimum_client_height > 0 &&
           edges_ != WindowResizeEdge::None;
}

Rect WindowResizeDrag::bounds_for(uint64_t pointer_x, uint64_t pointer_y) const
{
    if (!valid())
    {
        return {};
    }

    const uint64_t min_width = outer_width_for_client(constraints_);
    const uint64_t min_height = outer_height_for_client(constraints_);
    const uint64_t work_right = rect_right(constraints_.work_area);
    const uint64_t work_bottom = rect_bottom(constraints_.work_area);
    if (min_width == 0 || min_height == 0 || start_bounds_.x >= work_right ||
        start_bounds_.y >= work_bottom)
    {
        return {};
    }

    const uint64_t start_right = rect_right(start_bounds_);
    const uint64_t start_bottom = rect_bottom(start_bounds_);
    uint64_t left = start_bounds_.x;
    uint64_t right = start_right;
    uint64_t top = start_bounds_.y;
    uint64_t bottom = start_bottom;

    if (resize_edge_contains(edges_, WindowResizeEdge::Left))
    {
        if (start_right <= constraints_.work_area.x || start_right - constraints_.work_area.x < min_width)
        {
            return {};
        }
        const uint64_t requested_left = add_delta_clamped(start_bounds_.x, anchor_x_, pointer_x);
        if (start_right < min_width)
        {
            return {};
        }
        left = clamp_u64(requested_left, constraints_.work_area.x, start_right - min_width);
    }
    if (resize_edge_contains(edges_, WindowResizeEdge::Right))
    {
        if (work_right <= start_bounds_.x || work_right - start_bounds_.x < min_width)
        {
            return {};
        }
        const uint64_t requested_right = add_delta_clamped(start_right, anchor_x_, pointer_x);
        right = clamp_u64(requested_right, start_bounds_.x + min_width, work_right);
    }
    if (resize_edge_contains(edges_, WindowResizeEdge::Top))
    {
        if (start_bottom <= constraints_.work_area.y ||
            start_bottom - constraints_.work_area.y < min_height)
        {
            return {};
        }
        const uint64_t requested_top = add_delta_clamped(start_bounds_.y, anchor_y_, pointer_y);
        if (start_bottom < min_height)
        {
            return {};
        }
        top = clamp_u64(requested_top, constraints_.work_area.y, start_bottom - min_height);
    }
    if (resize_edge_contains(edges_, WindowResizeEdge::Bottom))
    {
        if (work_bottom <= start_bounds_.y || work_bottom - start_bounds_.y < min_height)
        {
            return {};
        }
        const uint64_t requested_bottom = add_delta_clamped(start_bottom, anchor_y_, pointer_y);
        bottom = clamp_u64(requested_bottom, start_bounds_.y + min_height, work_bottom);
    }

    if (right <= left || bottom <= top || right - left < min_width || bottom - top < min_height)
    {
        return {};
    }

    return {left, top, right - left, bottom - top};
}

WindowInteractionResult WindowInteractionController::start_interaction(WindowPointerEvent event)
{
    WindowInteractionResult result;
    result.cursor_shape = cursor_shape_for_hit_region(event.hit_region);

    if (event.current_bounds.empty())
    {
        return result;
    }

    if (event.hit_region == WindowChromeHitRegion::TitleBar)
    {
        result.focus_requested = true;
        move_drag_ =
            WindowMoveDrag::begin(event.current_bounds, event.x, event.y, event.constraints.work_area);
        result.handled = move_drag_.valid();
        if (result.handled)
        {
            mode_ = WindowInteractionMode::Move;
            active_cursor_shape_ = PointerCursorShape::Move;
            result.mode = mode_;
        }
        return result;
    }

    if (event.hit_region == WindowChromeHitRegion::Content)
    {
        result.handled = true;
        result.focus_requested = true;
        return result;
    }

    if (hit_region_is_resize(event.hit_region))
    {
        resize_drag_ = WindowResizeDrag::begin(event.current_bounds,
                                               event.x,
                                               event.y,
                                               resize_edges_for_hit_region(event.hit_region),
                                               event.constraints);
        result.handled = resize_drag_.valid();
        if (result.handled)
        {
            mode_ = WindowInteractionMode::Resize;
            active_cursor_shape_ = result.cursor_shape;
            result.mode = mode_;
        }
        return result;
    }

    if (event.hit_region == WindowChromeHitRegion::CloseButton)
    {
        mode_ = WindowInteractionMode::Close;
        active_cursor_shape_ = PointerCursorShape::Arrow;
        result.handled = true;
        result.mode = mode_;
        return result;
    }

    return result;
}

WindowInteractionResult WindowInteractionController::update_active(WindowPointerEvent event,
                                                                   bool released)
{
    WindowInteractionResult result;
    result.handled = true;
    result.mode = mode_;

    switch (mode_)
    {
    case WindowInteractionMode::Move:
        result.cursor_shape = active_cursor_shape_;
        result.proposed_bounds = move_drag_.bounds_for(event.x, event.y);
        result.commit_move = released && !result.proposed_bounds.empty();
        break;
    case WindowInteractionMode::Resize:
        result.cursor_shape = active_cursor_shape_;
        result.proposed_bounds = resize_drag_.bounds_for(event.x, event.y);
        result.commit_resize = released && !result.proposed_bounds.empty();
        break;
    case WindowInteractionMode::Close:
        result.cursor_shape = PointerCursorShape::Arrow;
        result.close_requested = released && event.hit_region == WindowChromeHitRegion::CloseButton;
        break;
    case WindowInteractionMode::None:
        result.handled = false;
        break;
    }

    if (released)
    {
        reset();
    }
    return result;
}

WindowInteractionResult WindowInteractionController::update(WindowPointerEvent event)
{
    const bool pressed = event.primary_down && !previous_primary_down_;
    const bool released = !event.primary_down && previous_primary_down_;
    previous_primary_down_ = event.primary_down;

    if (active())
    {
        return update_active(event, released);
    }

    if (pressed)
    {
        return start_interaction(event);
    }

    WindowInteractionResult result;
    result.cursor_shape = cursor_shape_for_hit_region(event.hit_region);
    return result;
}

void WindowInteractionController::reset()
{
    previous_primary_down_ = false;
    mode_ = WindowInteractionMode::None;
    active_cursor_shape_ = PointerCursorShape::Arrow;
    move_drag_ = {};
    resize_drag_ = {};
}

WindowInteractionReplay::WindowInteractionReplay(Rect desktop_bounds,
                                                 WindowFrameConfig frame_config)
    : desktop_bounds_(desktop_bounds)
    , frame_config_(frame_config)
{
}

WindowInteractionReplayStats WindowInteractionReplay::profile_preview_then_commit(
    WindowInteractionReplayConfig config) const
{
    WindowInteractionReplayStats stats;
    if (desktop_bounds_.empty() || config.start_bounds.empty() || config.step_count == 0)
    {
        return stats;
    }

    WindowInteractionController controller;
    WindowRepaintPlanner planner(desktop_bounds_, frame_config_);
    WindowPreviewShape preview_shape(desktop_bounds_, frame_config_);
    Rect preview_bounds;
    const WindowInteractionResult press = controller.update({
        config.start_bounds,
        config.press_region,
        config.press_x,
        config.press_y,
        true,
        config.constraints,
    });
    ++stats.pointer_event_count;
    if (!press.handled)
    {
        return stats;
    }

    Rect proposed_bounds;
    for (size_t step = 1; step <= config.step_count; ++step)
    {
        const uint64_t x = config.press_x + (config.step_delta_x * step);
        const uint64_t y = config.press_y + (config.step_delta_y * step);
        const WindowInteractionResult drag = controller.update({
            config.start_bounds,
            WindowChromeHitRegion::None,
            x,
            y,
            true,
            config.constraints,
        });
        ++stats.pointer_event_count;
        if (drag.proposed_bounds.empty())
        {
            continue;
        }

        proposed_bounds = drag.proposed_bounds;
        ++stats.preview_update_count;
        record_repaint_regions(stats, preview_shape.damage_for(preview_bounds), false);
        record_repaint_regions(stats, preview_shape.damage_for(proposed_bounds), false);
        preview_bounds = proposed_bounds;
    }

    const uint64_t release_x = config.press_x + (config.step_delta_x * config.step_count);
    const uint64_t release_y = config.press_y + (config.step_delta_y * config.step_count);
    const WindowInteractionResult release = controller.update({
        config.start_bounds,
        WindowChromeHitRegion::None,
        release_x,
        release_y,
        false,
        config.constraints,
    });
    ++stats.pointer_event_count;
    const bool committed = (config.kind == WindowInteractionReplayKind::Move && release.commit_move) ||
                           (config.kind == WindowInteractionReplayKind::Resize &&
                            release.commit_resize);
    if (committed && !release.proposed_bounds.empty())
    {
        ++stats.commit_count;
        record_repaint_regions(stats, planner.move_damage(config.start_bounds, release.proposed_bounds), true);
    }
    return stats;
}

} // namespace kernel::display
