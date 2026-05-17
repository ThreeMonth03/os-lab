#include "kernel/display/terminal_repaint_state.hpp"

namespace kernel::display
{

void TerminalRepaintState::reset(Rect bounds)
{
    bounds_ = bounds;
}

TerminalRepaintRequest TerminalRepaintState::record_dirty(Rect rect)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty())
    {
        return {};
    }

    FrameDamage damage;
    if (!damage.append_dirty(rect))
    {
        return {{rect, {}}};
    }
    return {damage};
}

TerminalRepaintRequest TerminalRepaintState::record_scroll(Rect rect, uint64_t distance)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty() || distance == 0)
    {
        return {};
    }

    FrameDamage damage;
    const ScrollDamage scroll{rect, distance};
    if (!damage.append_scroll(scroll))
    {
        return {{rect, {}}};
    }

    if (!damage.append_dirty(exposed_scroll_region(scroll), true))
    {
        return {{rect, {}}};
    }
    return {damage};
}

} // namespace kernel::display
