#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/frame_damage.hpp"

namespace kernel::display
{

struct DisplayFrameFlush
{
    bool outermost_frame_ended = false;
    FrameDamage damage;
};

struct DisplayFrameSubmit
{
    bool immediate = false;
    FrameDamage damage;
};

class DisplayFrame
{
public:
    DisplayFrame() = default;
    explicit DisplayFrame(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void begin();
    [[nodiscard]] DisplayFrameFlush end();
    [[nodiscard]] DisplayFrameSubmit submit(FrameDamage damage);

    bool in_frame() const { return depth_ > 0; }
    uint32_t depth() const { return depth_; }
    Rect bounds() const { return damage_.bounds(); }
    FrameDamage pending() const { return damage_.pending(); }

private:
    void accumulate(FrameDamage damage);

    uint32_t depth_ = 0;
    DamageAccumulator damage_;
};

} // namespace kernel::display
