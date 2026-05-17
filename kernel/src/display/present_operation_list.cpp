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
    if (intersect_rect(lhs, rhs).width > 0 || intersect_rect(lhs, rhs).height > 0)
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
    rect = intersect_rect(bounds_, rect);
    if (rect.empty())
    {
        return PresentOperationAppendResult::Ignored;
    }

    if (full_screen_fallback_)
    {
        return PresentOperationAppendResult::FullscreenFallback;
    }

    if (can_merge_last_rect(rect))
    {
        operations_[count_ - 1].rect = bounding_rect(operations_[count_ - 1].rect, rect);
        rebuild_stats();
        return PresentOperationAppendResult::Merged;
    }

    if (count_ >= kMaxPresentOperations)
    {
        return fallback_to_fullscreen();
    }

    operations_[count_] = {PresentOperationKind::Rect, rect, 0};
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

    if (count_ >= kMaxPresentOperations)
    {
        return fallback_to_fullscreen();
    }

    operations_[count_] = {PresentOperationKind::Scroll, rect, distance};
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

PresentOperationAppendResult PresentOperationList::fallback_to_fullscreen()
{
    for (auto & operation : operations_)
    {
        operation = {};
    }

    count_ = bounds_.empty() ? 0 : 1;
    if (count_ == 1)
    {
        operations_[0] = {PresentOperationKind::Rect, bounds_, 0};
    }
    full_screen_fallback_ = true;
    rebuild_stats();
    return PresentOperationAppendResult::FullscreenFallback;
}

bool PresentOperationList::can_merge_last_rect(Rect rect) const
{
    if (count_ == 0)
    {
        return false;
    }

    const PresentOperation & last = operations_[count_ - 1];
    return last.rect_present() && adjacent_or_overlapping(last.rect, rect);
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
