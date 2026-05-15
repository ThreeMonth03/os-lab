#include "kernel/display/terminal_repaint_state.hpp"

namespace kernel::display
{

void TerminalRepaintState::reset()
{
    update_depth_ = 0;
    clear_pending();
}

void TerminalRepaintState::begin_batch()
{
    const bool outermost = update_depth_ == 0;
    if (outermost)
    {
        clear_pending();
    }
    ++update_depth_;
}

TerminalRepaintFlush TerminalRepaintState::end_batch()
{
    if (update_depth_ == 0)
    {
        return {};
    }

    --update_depth_;
    if (update_depth_ > 0)
    {
        return {};
    }

    TerminalRepaintFlush flush = {};
    flush.outermost_batch_ended = true;
    flush.repaint_text_layer = pending_full_repaint_ || pending_scroll_rows_ > 0;
    flush.full_text_repaint = pending_full_repaint_;
    flush.repaint_higher_layers = pending_dirty_valid_;
    flush.scroll_rows = pending_full_repaint_ ? 0 : pending_scroll_rows_;
    flush.dirty_rect = pending_dirty_;
    clear_pending();
    return flush;
}

TerminalRepaintRequest TerminalRepaintState::record_dirty(Rect rect)
{
    if (rect.empty())
    {
        return {};
    }

    if (!in_batch())
    {
        return {false, false, true, 0, rect};
    }

    record_pending_dirty(rect);
    return {};
}

TerminalRepaintRequest TerminalRepaintState::record_scroll(Rect bounds, uint64_t visible_rows)
{
    if (bounds.empty())
    {
        return {};
    }

    if (!in_batch())
    {
        const bool full_repaint = visible_rows > 0 && visible_rows <= 1;
        return {true, full_repaint, true, full_repaint ? 0u : 1u, bounds};
    }

    record_pending_scroll(bounds, visible_rows);
    return {};
}

void TerminalRepaintState::clear_pending()
{
    pending_dirty_ = {};
    pending_dirty_valid_ = false;
    pending_full_repaint_ = false;
    pending_scroll_rows_ = 0;
}

void TerminalRepaintState::record_pending_dirty(Rect rect)
{
    if (rect.empty())
    {
        return;
    }

    if (pending_full_repaint_ && pending_dirty_valid_)
    {
        return;
    }

    if (!pending_dirty_valid_)
    {
        pending_dirty_ = rect;
        pending_dirty_valid_ = true;
        return;
    }

    pending_dirty_ = bounding_rect(pending_dirty_, rect);
}

void TerminalRepaintState::record_pending_full_repaint(Rect bounds)
{
    pending_full_repaint_ = true;
    pending_scroll_rows_ = 0;
    if (!pending_dirty_valid_)
    {
        pending_dirty_ = bounds;
        pending_dirty_valid_ = true;
        return;
    }

    pending_dirty_ = bounding_rect(pending_dirty_, bounds);
}

void TerminalRepaintState::record_pending_scroll(Rect bounds, uint64_t visible_rows)
{
    if (pending_full_repaint_)
    {
        record_pending_dirty(bounds);
        return;
    }

    ++pending_scroll_rows_;
    if (visible_rows > 0 && pending_scroll_rows_ >= visible_rows)
    {
        record_pending_full_repaint(bounds);
        return;
    }

    record_pending_dirty(bounds);
}

} // namespace kernel::display
