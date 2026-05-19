#include "kernel/display/window_manager.hpp"

namespace kernel::display
{

bool WindowManagerResult::valid() const
{
    return success && (!changed || session.valid());
}

void WindowManager::reset(WindowSessionHost & sessions, WindowStack & stack)
{
    sessions_ = &sessions;
    stack_ = &stack;
}

bool WindowManager::ready() const
{
    return sessions_ != nullptr && stack_ != nullptr && sessions_->ready();
}

const WindowSession * WindowManager::find_window(WindowSessionId id) const
{
    return ready() ? sessions_->find(id) : nullptr;
}

WindowManagerSnapshot WindowManager::snapshot() const
{
    if (!ready())
    {
        return {};
    }

    return {
        stack_->focused_window(),
        stack_->active_window(),
        stack_->topmost_visible_window(),
        stack_->size(),
    };
}

bool WindowManager::selected(WindowSessionId id) const
{
    const WindowSession * session = find_window(id);
    return session != nullptr && session->visible() && session->focused && session->active &&
           ready() && stack_->topmost_visible_window() == id;
}

bool WindowManager::show_window(WindowSessionId id, WindowManagerResult & result)
{
    result = {};
    const WindowSession * session = find_window(id);
    if (session == nullptr || session->closed())
    {
        return false;
    }
    if (session->visible())
    {
        set_noop(result);
        return true;
    }

    WindowSessionMutation mutation;
    return sessions_->set_visible(id, true, mutation) &&
           finish_session_mutation(mutation, StackPolicy::Sync, result);
}

bool WindowManager::hide_window(WindowSessionId id, WindowManagerResult & result)
{
    result = {};
    const WindowSession * session = find_window(id);
    if (session == nullptr || session->closed())
    {
        return false;
    }
    if (!session->visible())
    {
        set_noop(result);
        return true;
    }

    WindowSessionMutation mutation;
    return sessions_->set_visible(id, false, mutation) &&
           finish_session_mutation(mutation, StackPolicy::Sync, result);
}

bool WindowManager::clear_focus(WindowSessionId id, WindowManagerResult & result)
{
    result = {};
    const WindowSession * session = find_window(id);
    if (session == nullptr || session->closed())
    {
        return false;
    }
    if (!session->focused && !session->active)
    {
        set_noop(result);
        return true;
    }

    WindowSessionMutation mutation;
    return sessions_->clear_focus(id, mutation) &&
           finish_session_mutation(mutation, StackPolicy::Sync, result);
}

bool WindowManager::focus_window(WindowSessionId id, WindowManagerResult & result)
{
    result = {};
    const WindowSession * session = find_window(id);
    if (session == nullptr || !session->visible())
    {
        return false;
    }
    if (session->focused && snapshot().focused == id)
    {
        set_noop(result);
        return true;
    }

    WindowSessionMutation mutation;
    return sessions_->focus_session(id, mutation) &&
           finish_session_mutation(mutation, StackPolicy::Focus, result);
}

bool WindowManager::activate_window(WindowSessionId id, WindowManagerResult & result)
{
    return stack_only_mutation(id, StackPolicy::Activate, result);
}

bool WindowManager::raise_window(WindowSessionId id, WindowManagerResult & result)
{
    return stack_only_mutation(id, StackPolicy::Raise, result);
}

bool WindowManager::focus_activate_raise_window(WindowSessionId id,
                                                WindowManagerResult & result)
{
    result = {};
    if (selected(id))
    {
        set_noop(result);
        return true;
    }

    return stack_only_mutation(id, StackPolicy::FocusActivateRaise, result);
}

bool WindowManager::show_focus_activate_raise_window(WindowSessionId id,
                                                     WindowManagerResult & result)
{
    result = {};
    const WindowSession * session = find_window(id);
    if (session == nullptr || session->closed())
    {
        return false;
    }
    if (selected(id))
    {
        set_noop(result);
        return true;
    }
    if (session->visible())
    {
        return focus_activate_raise_window(id, result);
    }

    WindowSessionMutation mutation;
    return sessions_->set_visible(id, true, mutation) &&
           finish_session_mutation(mutation, StackPolicy::FocusActivateRaise, result);
}

bool WindowManager::move_window(WindowSessionId id,
                                WindowSessionBounds bounds,
                                WindowManagerResult & result)
{
    result = {};
    WindowSessionMutation mutation;
    return sessions_->move_session(id, bounds, mutation) &&
           finish_session_mutation(mutation, StackPolicy::Sync, result);
}

bool WindowManager::resize_window(WindowSessionId id,
                                  WindowSessionBounds bounds,
                                  WindowManagerResult & result)
{
    result = {};
    WindowSessionMutation mutation;
    return sessions_->resize_session(id, bounds, mutation) &&
           finish_session_mutation(mutation, StackPolicy::Sync, result);
}

bool WindowManager::close_window(WindowSessionId id, WindowManagerResult & result)
{
    result = {};
    WindowSessionMutation mutation;
    return sessions_->close_session(id, mutation) &&
           finish_session_mutation(mutation, StackPolicy::Sync, result);
}

bool WindowManager::finish_session_mutation(WindowSessionMutation mutation,
                                            StackPolicy policy,
                                            WindowManagerResult & result)
{
    result = {};
    if (!ready() || !mutation.valid())
    {
        return false;
    }

    const WindowStack previous_stack = *stack_;
    if (!apply_stack_policy(mutation.current, policy))
    {
        if (!rollback(mutation.previous, mutation.app_surface.previous, previous_stack))
        {
            return false;
        }
        return false;
    }

    if (!mutation.current.closed() &&
        !sessions_->restore_session(mutation.current, mutation.current.app_surface()))
    {
        if (!rollback(mutation.previous, mutation.app_surface.previous, previous_stack))
        {
            return false;
        }
        return false;
    }
    if (!sync_sessions_from_stack())
    {
        if (!rollback(mutation.previous, mutation.app_surface.previous, previous_stack))
        {
            return false;
        }
        return false;
    }
    if (const WindowSession * synced = sessions_->find(mutation.current.id); synced != nullptr)
    {
        mutation.current = *synced;
        mutation.app_surface.current = synced->app_surface();
    }

    result = {
        true,
        true,
        mutation,
        previous_stack,
        *stack_,
    };
    return true;
}

bool WindowManager::apply_stack_policy(WindowSession & session, StackPolicy policy)
{
    if (!ready() || !stack_->sync_window(session))
    {
        return false;
    }

    bool policy_applied = true;
    switch (policy)
    {
    case StackPolicy::Sync:
        break;
    case StackPolicy::Focus:
        policy_applied = stack_->focus_window(session.id);
        break;
    case StackPolicy::Activate:
        policy_applied = stack_->activate_window(session.id);
        break;
    case StackPolicy::Raise:
        policy_applied = stack_->raise_to_front(session.id);
        break;
    case StackPolicy::FocusActivateRaise:
        policy_applied = stack_->focus_and_activate(session.id) &&
                         stack_->raise_to_front(session.id);
        break;
    }

    if (!policy_applied)
    {
        return false;
    }

    const WindowStackEntry * entry = stack_->find(session.id);
    if (entry == nullptr)
    {
        return false;
    }

    session.state = entry->state;
    session.focused = entry->focused;
    session.active = entry->active;
    return true;
}

bool WindowManager::sync_sessions_from_stack()
{
    if (!ready())
    {
        return false;
    }

    for (size_t index = 0; index < stack_->size(); ++index)
    {
        const WindowStackEntry * entry = stack_->at(index);
        const WindowSession * current =
            entry == nullptr ? nullptr : sessions_->find(entry->id);
        if (current == nullptr || current->closed())
        {
            continue;
        }

        WindowSession next = *current;
        next.state = entry->state;
        next.focused = entry->focused;
        next.active = entry->active;
        if (!sessions_->restore_session(next, next.app_surface()))
        {
            return false;
        }
    }

    return true;
}

bool WindowManager::stack_only_mutation(WindowSessionId id,
                                        StackPolicy policy,
                                        WindowManagerResult & result)
{
    result = {};
    const WindowSession * session = find_window(id);
    if (session == nullptr || !session->visible())
    {
        return false;
    }

    WindowSession current = *session;
    const WindowSession previous = current;
    const AppSurface previous_app = previous.app_surface();
    const WindowStack previous_stack = *stack_;
    if (!apply_stack_policy(current, policy))
    {
        *stack_ = previous_stack;
        return false;
    }

    const AppSurface current_app = current.app_surface();
    if (!sessions_->restore_session(current, current_app) || !sync_sessions_from_stack())
    {
        if (!rollback(previous, previous_app, previous_stack))
        {
            return false;
        }
        return false;
    }
    const WindowSession * synced = sessions_->find(id);
    if (synced == nullptr)
    {
        if (!rollback(previous, previous_app, previous_stack))
        {
            return false;
        }
        return false;
    }
    current = *synced;

    result = {
        true,
        true,
        {previous, current, {previous_app, current.app_surface(), {}}, current.bounds.outer},
        previous_stack,
        *stack_,
    };
    return true;
}

void WindowManager::set_noop(WindowManagerResult & result) const
{
    result = {
        true,
        false,
        {},
        ready() ? *stack_ : WindowStack{},
        ready() ? *stack_ : WindowStack{},
    };
}

bool WindowManager::rollback(WindowSession previous,
                             AppSurface previous_app,
                             WindowStack previous_stack)
{
    if (!ready() || !sessions_->restore_session(previous, previous_app))
    {
        return false;
    }

    *stack_ = previous_stack;
    return true;
}

} // namespace kernel::display
