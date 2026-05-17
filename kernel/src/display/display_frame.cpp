#include "kernel/display/display_frame.hpp"

namespace kernel::display
{

namespace
{

uint64_t rect_area(Rect rect)
{
    return rect.width * rect.height;
}

} // namespace

void DisplayFrame::reset(Rect bounds)
{
    depth_ = 0;
    bounds_ = bounds;
    present_regions_.reset(bounds);
    stats_ = {};
}

void DisplayFrame::begin()
{
    if (depth_ == 0)
    {
        present_regions_.clear();
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

    const PresentRegionList regions = present_regions_;
    record_stats(regions, true);
    present_regions_.clear();
    return {true, regions};
}

DisplayFrameSubmit DisplayFrame::submit(Rect present_rect)
{
    PresentRegionList regions(bounds_);
    regions.append(present_rect);
    return submit(regions);
}

DisplayFrameSubmit DisplayFrame::submit(const PresentRegionList & present_regions)
{
    if (present_regions.empty())
    {
        return {};
    }

    if (!in_frame())
    {
        record_stats(present_regions, false);
        return {true, present_regions};
    }

    for (size_t index = 0; index < present_regions.count(); ++index)
    {
        present_regions_.append(present_regions.at(index));
    }
    return {};
}

void DisplayFrame::reset_stats()
{
    stats_ = {};
}

void DisplayFrame::record_stats(const PresentRegionList & present_regions, bool frame_flush)
{
    if (frame_flush)
    {
        ++stats_.frame_flush_count;
    }

    if (present_regions.full_screen_fallback())
    {
        ++stats_.large_present_fallback_count;
    }

    stats_.present_rect_count += present_regions.count();
    for (size_t index = 0; index < present_regions.count(); ++index)
    {
        const uint64_t area = rect_area(present_regions.at(index));
        stats_.total_presented_pixels += area;
        if (area > stats_.largest_present_rect_area)
        {
            stats_.largest_present_rect_area = area;
        }
    }
}

} // namespace kernel::display
