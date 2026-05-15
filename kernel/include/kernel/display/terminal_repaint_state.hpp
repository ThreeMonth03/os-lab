#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

struct TerminalRepaintRequest
{
    bool repaint_text_layer = false;
    bool full_text_repaint = false;
    bool repaint_higher_layers = false;
    uint64_t scroll_rows = 0;
    Rect dirty_rect;
};

struct TerminalRepaintFlush
{
    bool outermost_batch_ended = false;
    bool repaint_text_layer = false;
    bool full_text_repaint = false;
    bool repaint_higher_layers = false;
    uint64_t scroll_rows = 0;
    Rect dirty_rect;
};

class TerminalRepaintState
{
public:
    TerminalRepaintState() = default;

    void reset();
    void begin_batch();
    [[nodiscard]] TerminalRepaintFlush end_batch();
    [[nodiscard]] TerminalRepaintRequest record_dirty(Rect rect);
    [[nodiscard]] TerminalRepaintRequest record_scroll(Rect bounds, uint64_t visible_rows = 0);

    bool in_batch() const { return update_depth_ > 0; }
    bool pending_text_repaint() const { return pending_full_repaint_ || pending_scroll_rows_ > 0; }
    bool pending_full_text_repaint() const { return pending_full_repaint_; }
    uint64_t pending_scroll_rows() const { return pending_scroll_rows_; }
    uint32_t update_depth() const { return update_depth_; }

private:
    void clear_pending();
    void record_pending_dirty(Rect rect);
    void record_pending_full_repaint(Rect bounds);
    void record_pending_scroll(Rect bounds, uint64_t visible_rows);

    uint32_t update_depth_ = 0;
    Rect pending_dirty_;
    bool pending_dirty_valid_ = false;
    bool pending_full_repaint_ = false;
    uint64_t pending_scroll_rows_ = 0;
};

} // namespace kernel::display
