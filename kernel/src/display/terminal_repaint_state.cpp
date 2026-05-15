#include "kernel/display/terminal_repaint_state.hpp"

namespace kernel::display
{

void TerminalRepaintState::reset()
{
    update_depth_ = 0;
    clear_pending();
}

bool TerminalRepaintState::begin_batch()
{
    const bool outermost = update_depth_ == 0;
    if (outermost)
    {
        clear_pending();
    }
    ++update_depth_;
    return outermost;
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
    flush.repaint_text_layer = pending_full_repaint_;
    flush.repaint_higher_layers = pending_dirty_valid_;
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
        return {false, true, rect};
    }

    record_pending_dirty(rect);
    return {};
}

TerminalRepaintRequest TerminalRepaintState::record_scroll(Rect bounds)
{
    if (bounds.empty())
    {
        return {};
    }

    if (!in_batch())
    {
        return {true, true, bounds};
    }

    record_pending_full_repaint(bounds);
    return {};
}

void TerminalRepaintState::clear_pending()
{
    pending_dirty_ = {};
    pending_dirty_valid_ = false;
    pending_full_repaint_ = false;
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
    if (!pending_dirty_valid_)
    {
        pending_dirty_ = bounds;
        pending_dirty_valid_ = true;
        return;
    }

    pending_dirty_ = bounding_rect(pending_dirty_, bounds);
}

} // namespace kernel::display
