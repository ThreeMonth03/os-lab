#include "kernel/display/terminal_repaint_state.hpp"

namespace kernel::display
{

void TerminalRepaintState::reset(Rect bounds)
{
    update_depth_ = 0;
    damage_.reset(bounds);
}

void TerminalRepaintState::begin_batch()
{
    if (update_depth_ == 0)
    {
        damage_.clear();
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

    return {
        true,
        damage_.flush(),
    };
}

TerminalRepaintFlush TerminalRepaintState::flush_pending()
{
    return {
        false,
        damage_.flush(),
    };
}

TerminalRepaintRequest TerminalRepaintState::record_dirty(Rect rect)
{
    if (rect.empty())
    {
        return {};
    }

    if (!in_batch())
    {
        return {{rect, {}}};
    }

    damage_.mark_dirty(rect);
    return {};
}

TerminalRepaintRequest TerminalRepaintState::record_scroll(Rect rect, uint64_t distance)
{
    if (rect.empty() || distance == 0)
    {
        return {};
    }

    if (!in_batch())
    {
        return {{
            exposed_scroll_region({rect, distance}),
            {rect, distance},
        }};
    }

    damage_.record_scroll(rect, distance);
    return {};
}

} // namespace kernel::display
