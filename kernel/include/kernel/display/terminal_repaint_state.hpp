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

struct TerminalRepaintFlush
{
    bool outermost_batch_ended = false;
    FrameDamage damage;
};

class TerminalRepaintState
{
public:
    TerminalRepaintState() = default;

    void reset(Rect bounds);
    void begin_batch();
    [[nodiscard]] TerminalRepaintFlush end_batch();
    [[nodiscard]] TerminalRepaintFlush flush_pending();
    [[nodiscard]] TerminalRepaintRequest record_dirty(Rect rect);
    [[nodiscard]] TerminalRepaintRequest record_scroll(Rect rect, uint64_t distance);

    bool in_batch() const { return update_depth_ > 0; }
    bool pending_damage() const { return !damage_.empty(); }
    uint32_t update_depth() const { return update_depth_; }

private:
    uint32_t update_depth_ = 0;
    DamageAccumulator damage_;
};

} // namespace kernel::display
