#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/frame_damage.hpp"

namespace kernel::display
{

struct TerminalRepaintRequest
{
    FrameDamage damage;
};

class TerminalRepaintState
{
public:
    TerminalRepaintState() = default;

    void reset(Rect bounds);
    [[nodiscard]] TerminalRepaintRequest record_dirty(Rect rect);
    [[nodiscard]] TerminalRepaintRequest record_scroll(Rect rect, uint64_t distance);

    Rect bounds() const { return bounds_; }

private:
    Rect bounds_;
};

} // namespace kernel::display
