#include "kernel/display/display_frame.hpp"

namespace kernel::display
{

void DisplayFrame::reset(Rect bounds)
{
    depth_ = 0;
    bounds_ = bounds;
    present_operations_.reset(bounds);
    stats_ = {};
}

void DisplayFrame::begin()
{
    if (depth_ == 0)
    {
        present_operations_.clear();
    }
    ++depth_;
}

DisplayFrameFlush DisplayFrame::end()
{
    if (depth_ == 0)
    {
        return {};
    }

    --depth_;
    if (depth_ > 0)
    {
        return {};
    }

    PresentOperationList operations = present_operations_;
    operations.compact_complex_scrolls_to_rect();
    record_stats(operations, true);
    present_operations_.clear();
    return {true, operations};
}

DisplayFrameSubmit DisplayFrame::submit(Rect present_rect)
{
    PresentOperationList operations(bounds_);
    operations.append_rect(present_rect);
    return submit(operations);
}

DisplayFrameSubmit DisplayFrame::submit(const PresentOperationList & present_operations)
{
    if (present_operations.empty())
    {
        return {};
    }

    if (!in_frame())
    {
        record_stats(present_operations, false);
        return {true, present_operations};
    }

    for (size_t index = 0; index < present_operations.count(); ++index)
    {
        const PresentOperation operation = present_operations.at(index);
        if (operation.rect_present())
        {
            if (operation.scroll_repair_rect_present())
            {
                present_operations_.append_scroll_repair_rect(operation.rect);
            }
            else if (operation.scroll_exposed_rect_present())
            {
                present_operations_.append_scroll_exposed_rect(operation.rect);
            }
            else
            {
                present_operations_.append_rect(operation.rect);
            }
        }
        else if (operation.scroll_present())
        {
            present_operations_.append_scroll(operation.rect, operation.distance);
        }
    }
    return {};
}

void DisplayFrame::reset_stats()
{
    stats_ = {};
}

void DisplayFrame::record_stats(const PresentOperationList & present_operations, bool frame_flush)
{
    if (frame_flush)
    {
        ++stats_.frame_flush_count;
    }

    const PresentOperationStats operation_stats = present_operations.stats();
    if (present_operations.full_screen_fallback())
    {
        ++stats_.large_present_fallback_count;
    }

    stats_.present_operation_count += operation_stats.operation_count;
    stats_.present_rect_count += operation_stats.rect_count;
    stats_.present_scroll_count += operation_stats.scroll_count;
    stats_.total_presented_pixels += operation_stats.total_presented_pixels;
    if (operation_stats.largest_present_rect_area > stats_.largest_present_rect_area)
    {
        stats_.largest_present_rect_area = operation_stats.largest_present_rect_area;
    }
}

} // namespace kernel::display
