#include "kernel/display/display_frame.hpp"

namespace kernel::display
{

void DisplayFrame::reset(Rect bounds)
{
    depth_ = 0;
    bounds_ = bounds;
    present_rect_ = {};
}

void DisplayFrame::begin()
{
    if (depth_ == 0)
    {
        present_rect_ = {};
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

    const Rect rect = present_rect_;
    present_rect_ = {};
    return {true, rect};
}

DisplayFrameSubmit DisplayFrame::submit(Rect present_rect)
{
    present_rect = intersect_rect(bounds_, present_rect);
    if (present_rect.empty())
    {
        return {};
    }

    if (!in_frame())
    {
        return {true, present_rect};
    }

    present_rect_ = bounding_rect(present_rect_, present_rect);
    return {};
}

} // namespace kernel::display
