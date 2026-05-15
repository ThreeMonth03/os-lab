#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

struct TerminalRepaintRequest
{
    bool repaint_text_layer = false;
    bool repaint_higher_layers = false;
    Rect dirty_rect;
};

struct TerminalRepaintFlush
{
    bool outermost_batch_ended = false;
    bool repaint_text_layer = false;
    bool repaint_higher_layers = false;
    Rect dirty_rect;
};

class TerminalRepaintState
{
public:
    TerminalRepaintState() = default;

    void reset();
    [[nodiscard]] bool begin_batch();
    [[nodiscard]] TerminalRepaintFlush end_batch();
    [[nodiscard]] TerminalRepaintRequest record_dirty(Rect rect);
    [[nodiscard]] TerminalRepaintRequest record_scroll(Rect bounds);

    bool in_batch() const { return update_depth_ > 0; }
    bool pending_full_repaint() const { return pending_full_repaint_; }
    uint32_t update_depth() const { return update_depth_; }

private:
    void clear_pending();
    void record_pending_dirty(Rect rect);
    void record_pending_full_repaint(Rect bounds);

    uint32_t update_depth_ = 0;
    Rect pending_dirty_;
    bool pending_dirty_valid_ = false;
    bool pending_full_repaint_ = false;
};

} // namespace kernel::display
