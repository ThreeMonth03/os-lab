#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

struct DisplayFrameFlush
{
    bool outermost_frame_ended = false;
    Rect present_rect;
};

struct DisplayFrameSubmit
{
    bool immediate = false;
    Rect present_rect;
};

class DisplayFrame
{
public:
    DisplayFrame() = default;
    explicit DisplayFrame(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void begin();
    [[nodiscard]] DisplayFrameFlush end();
    [[nodiscard]] DisplayFrameSubmit submit(Rect present_rect);

    bool in_frame() const { return depth_ > 0; }
    uint32_t depth() const { return depth_; }
    Rect bounds() const { return bounds_; }
    Rect pending() const { return present_rect_; }

private:
    uint32_t depth_ = 0;
    Rect bounds_;
    Rect present_rect_;
};

} // namespace kernel::display
