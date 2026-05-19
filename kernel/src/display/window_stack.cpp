#include "kernel/display/window_stack.hpp"

namespace
{

kernel::display::WindowStackEntry entry_for(kernel::display::WindowSession session)
{
    const bool visible = session.visible();
    return {
        session.id,
        session.state,
        visible && session.focused,
        visible && session.active,
    };
}

} // namespace

namespace kernel::display
{

void WindowStack::clear()
{
    for (size_t index = 0; index < count_; ++index)
    {
        entries_[index] = {};
    }
    count_ = 0;
}

bool WindowStack::register_window(WindowSession session)
{
    if (!session.valid() || session.closed() || contains(session.id) || count_ >= kMaxWindowSessions)
    {
        return false;
    }

    entries_[count_++] = entry_for(session);
    if (session.focused && !focus_window(session.id))
    {
        --count_;
        entries_[count_] = {};
        return false;
    }
    if (session.active && !activate_window(session.id))
    {
        --count_;
        entries_[count_] = {};
        return false;
    }
    return true;
}

bool WindowStack::sync_window(WindowSession session)
{
    if (!session.valid())
    {
        return false;
    }

    WindowStackEntry * entry = find_mutable(session.id);
    if (entry == nullptr)
    {
        return false;
    }

    *entry = entry_for(session);
    if (entry->focused)
    {
        clear_focus();
        entry->focused = true;
    }
    if (entry->active)
    {
        clear_active();
        entry->active = true;
    }
    return true;
}

bool WindowStack::set_visible(WindowSessionId id, bool visible)
{
    WindowStackEntry * entry = find_mutable(id);
    if (entry == nullptr || entry->closed())
    {
        return false;
    }

    entry->state = visible ? WindowSessionState::Open : WindowSessionState::Hidden;
    if (!visible)
    {
        entry->focused = false;
        entry->active = false;
    }
    return true;
}

bool WindowStack::close_window(WindowSessionId id)
{
    WindowStackEntry * entry = find_mutable(id);
    if (entry == nullptr || entry->closed())
    {
        return false;
    }

    entry->state = WindowSessionState::Closed;
    entry->focused = false;
    entry->active = false;
    return true;
}

bool WindowStack::focus_window(WindowSessionId id)
{
    WindowStackEntry * entry = find_mutable(id);
    if (entry == nullptr || !entry->open())
    {
        return false;
    }

    clear_focus();
    entry->focused = true;
    return true;
}

bool WindowStack::activate_window(WindowSessionId id)
{
    WindowStackEntry * entry = find_mutable(id);
    if (entry == nullptr || !entry->open())
    {
        return false;
    }

    clear_active();
    entry->active = true;
    return true;
}

bool WindowStack::focus_and_activate(WindowSessionId id)
{
    return focus_window(id) && activate_window(id);
}

bool WindowStack::raise_to_front(WindowSessionId id)
{
    const size_t index = find_index(id);
    if (index >= count_ || !entries_[index].open())
    {
        return false;
    }

    const WindowStackEntry raised = entries_[index];
    for (size_t current = index; current + 1 < count_; ++current)
    {
        entries_[current] = entries_[current + 1];
    }
    entries_[count_ - 1] = raised;
    return true;
}

void WindowStack::clear_focus()
{
    for (size_t index = 0; index < count_; ++index)
    {
        entries_[index].focused = false;
    }
}

void WindowStack::clear_active()
{
    for (size_t index = 0; index < count_; ++index)
    {
        entries_[index].active = false;
    }
}

bool WindowStack::contains(WindowSessionId id) const
{
    return find(id) != nullptr;
}

const WindowStackEntry * WindowStack::find(WindowSessionId id) const
{
    const size_t index = find_index(id);
    return index < count_ ? &entries_[index] : nullptr;
}

const WindowStackEntry * WindowStack::at(size_t index) const
{
    return index < count_ ? &entries_[index] : nullptr;
}

WindowSessionId WindowStack::focused_window() const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (entries_[index].focused && entries_[index].open())
        {
            return entries_[index].id;
        }
    }
    return kInvalidWindowSessionId;
}

WindowSessionId WindowStack::active_window() const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (entries_[index].active && entries_[index].open())
        {
            return entries_[index].id;
        }
    }
    return kInvalidWindowSessionId;
}

WindowSessionId WindowStack::topmost_visible_window() const
{
    for (size_t index = count_; index > 0; --index)
    {
        const WindowStackEntry & entry = entries_[index - 1];
        if (entry.open())
        {
            return entry.id;
        }
    }
    return kInvalidWindowSessionId;
}

WindowStackEntry * WindowStack::find_mutable(WindowSessionId id)
{
    const size_t index = find_index(id);
    return index < count_ ? &entries_[index] : nullptr;
}

size_t WindowStack::find_index(WindowSessionId id) const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (entries_[index].id == id)
        {
            return index;
        }
    }
    return count_;
}

} // namespace kernel::display
