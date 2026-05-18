#include "kernel/display/present_operation_list.hpp"

namespace kernel::display
{

namespace
{

uint64_t rect_area(Rect rect)
{
    return rect.width * rect.height;
}

bool same_horizontal_span(Rect lhs, Rect rhs)
{
    return lhs.x == rhs.x && lhs.width == rhs.width;
}

bool same_vertical_span(Rect lhs, Rect rhs)
{
    return lhs.y == rhs.y && lhs.height == rhs.height;
}

bool adjacent_or_overlapping(Rect lhs, Rect rhs)
{
    if (!intersect_rect(lhs, rhs).empty())
    {
        return true;
    }

    if (same_horizontal_span(lhs, rhs) && (lhs.y + lhs.height == rhs.y || rhs.y + rhs.height == lhs.y))
    {
        return true;
    }

    if (same_vertical_span(lhs, rhs) && (lhs.x + lhs.width == rhs.x || rhs.x + rhs.width == lhs.x))
    {
        return true;
    }

    return false;
}

bool same_rect(Rect lhs, Rect rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width &&
           lhs.height == rhs.height;
}

bool scroll_aftermath_rect(PresentOperation operation)
{
    return operation.scroll_repair_rect_present() || operation.scroll_exposed_rect_present();
}

} // namespace

void PresentOperationList::reset(Rect bounds)
{
    bounds_ = bounds;
    clear();
}

void PresentOperationList::clear()
{
    for (auto & operation : operations_)
    {
        operation = {};
    }
    count_ = 0;
    full_screen_fallback_ = false;
    stats_ = {};
}

PresentOperationAppendResult PresentOperationList::append_rect(Rect rect)
{
    return append_rect(rect, PresentRectKind::Normal);
}

PresentOperationAppendResult PresentOperationList::append_scroll_repair_rect(Rect rect)
{
    return append_rect(rect, PresentRectKind::ScrollRepair);
}

PresentOperationAppendResult PresentOperationList::append_scroll_exposed_rect(Rect rect)
{
    return append_rect(rect, PresentRectKind::ScrollExposed);
}

PresentOperationAppendResult PresentOperationList::append_rect(Rect rect,
                                                               PresentRectKind rect_kind)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty())
    {
        return PresentOperationAppendResult::Ignored;
    }

    if (full_screen_fallback_)
    {
        return PresentOperationAppendResult::FullscreenFallback;
    }

    if (rect_kind != PresentRectKind::Normal && try_merge_scroll_aftermath_rect(rect, rect_kind))
    {
        return PresentOperationAppendResult::Merged;
    }

    if (can_merge_last_rect(rect, rect_kind))
    {
        operations_[count_ - 1].rect = bounding_rect(operations_[count_ - 1].rect, rect);
        rebuild_stats();
        return PresentOperationAppendResult::Merged;
    }

    if (count_ >= kMaxPresentOperations)
    {
        return fallback_to_fullscreen();
    }

    operations_[count_] = {PresentOperationKind::Rect, rect_kind, rect, 0};
    ++count_;
    rebuild_stats();
    return PresentOperationAppendResult::Queued;
}

PresentOperationAppendResult PresentOperationList::append_scroll(Rect rect, uint64_t distance)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty() || distance == 0)
    {
        return PresentOperationAppendResult::Ignored;
    }

    if (distance >= rect.height)
    {
        return append_rect(rect);
    }

    if (full_screen_fallback_)
    {
        return PresentOperationAppendResult::FullscreenFallback;
    }

    if (try_merge_scroll(rect, distance))
    {
        return PresentOperationAppendResult::Merged;
    }

    if (count_ >= kMaxPresentOperations)
    {
        return fallback_to_fullscreen();
    }

    operations_[count_] = {PresentOperationKind::Scroll, PresentRectKind::Normal, rect, distance};
    ++count_;
    rebuild_stats();
    return PresentOperationAppendResult::Queued;
}

PresentOperation PresentOperationList::at(size_t index) const
{
    if (index >= count_)
    {
        return {};
    }
    return operations_[index];
}

bool PresentOperationList::compact_complex_scrolls_to_rect()
{
    if (stats_.scroll_count <= 1 || full_screen_fallback_)
    {
        return false;
    }

    Rect compact_rect;
    for (size_t index = 0; index < count_; ++index)
    {
        const PresentOperation operation = operations_[index];
        if (operation.rect_present() || operation.scroll_present())
        {
            compact_rect = bounding_rect(compact_rect, operation.rect);
        }
    }

    if (compact_rect.empty())
    {
        return false;
    }

    replace_with_rect(compact_rect);
    return true;
}

void PresentOperationList::replace_with_rect(Rect rect)
{
    for (auto & operation : operations_)
    {
        operation = {};
    }

    rect = intersect_rect(bounds_, rect);
    count_ = rect.empty() ? 0 : 1;
    if (count_ == 1)
    {
        operations_[0] = {PresentOperationKind::Rect, PresentRectKind::Normal, rect, 0};
    }
    rebuild_stats();
}

PresentOperationAppendResult PresentOperationList::fallback_to_fullscreen()
{
    full_screen_fallback_ = true;
    replace_with_rect(bounds_);
    return PresentOperationAppendResult::FullscreenFallback;
}

bool PresentOperationList::can_merge_last_rect(Rect rect, PresentRectKind rect_kind) const
{
    if (count_ == 0)
    {
        return false;
    }

    const PresentOperation & last = operations_[count_ - 1];
    return last.rect_present() && last.rect_kind == rect_kind &&
           adjacent_or_overlapping(last.rect, rect);
}

bool PresentOperationList::try_merge_scroll_aftermath_rect(Rect rect, PresentRectKind rect_kind)
{
    if (count_ == 0)
    {
        return false;
    }

    size_t candidate = count_;
    while (candidate > 0 && scroll_aftermath_rect(operations_[candidate - 1]))
    {
        PresentOperation & operation = operations_[candidate - 1];
        if (operation.rect_kind == rect_kind && adjacent_or_overlapping(operation.rect, rect))
        {
            operation.rect = bounding_rect(operation.rect, rect);
            rebuild_stats();
            return true;
        }
        --candidate;
    }

    return false;
}

bool PresentOperationList::try_merge_scroll(Rect rect, uint64_t distance)
{
    if (count_ == 0)
    {
        return false;
    }

    size_t candidate = count_;
    while (candidate > 0)
    {
        PresentOperation & operation = operations_[candidate - 1];
        if (scroll_aftermath_rect(operation))
        {
            --candidate;
            continue;
        }

        if (!operation.scroll_present())
        {
            return false;
        }

        if (!same_rect(operation.rect, rect))
        {
            --candidate;
            continue;
        }

        const uint64_t merged_distance = operation.distance + distance;
        if (merged_distance >= rect.height)
        {
            operation = {PresentOperationKind::Rect, PresentRectKind::Normal, rect, 0};
        }
        else
        {
            operation.distance = merged_distance;
        }
        rebuild_stats();
        return true;
    }

    return false;
}

void PresentOperationList::rebuild_stats()
{
    stats_ = {};
    stats_.large_present_fallback_count = full_screen_fallback_ ? 1 : 0;

    for (size_t index = 0; index < count_; ++index)
    {
        const PresentOperation operation = operations_[index];
        if (operation.rect_present())
        {
            ++stats_.operation_count;
            ++stats_.rect_count;
            const uint64_t area = rect_area(operation.rect);
            stats_.total_presented_pixels += area;
            if (area > stats_.largest_present_rect_area)
            {
                stats_.largest_present_rect_area = area;
            }
        }
        else if (operation.scroll_present())
        {
            ++stats_.operation_count;
            ++stats_.scroll_count;
            const Rect copied = {
                operation.rect.x,
                operation.rect.y,
                operation.rect.width,
                operation.rect.height - operation.distance,
            };
            stats_.total_presented_pixels += rect_area(copied);
        }
    }
}

} // namespace kernel::display
