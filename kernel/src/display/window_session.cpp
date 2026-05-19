#include "kernel/display/window_session.hpp"

namespace kernel::display
{

bool WindowSessionBounds::valid() const
{
    return !outer.empty() && !client.empty();
}

bool WindowSession::valid() const
{
    return id != kInvalidWindowSessionId && app_surface_id != kInvalidAppSurfaceId &&
           bounds.valid();
}

AppSurface WindowSession::app_surface() const
{
    return make_app_surface(app_surface_id, bounds.outer, visible(), focused && visible());
}

CompositedSurfaceDescriptor WindowSession::composited_surface() const
{
    if (closed())
    {
        return {};
    }

    CompositedSurfaceDescriptor surface =
        make_composited_surface(app_surface_display_id_for(app_surface_id),
                                CompositedSurfaceRole::App,
                                bounds.outer,
                                visible(),
                                active && visible(),
                                focused && visible());
    surface.occlusion = LayerOcclusion::Opaque;
    return surface;
}

WindowSession make_terminal_window_session(WindowSessionId id,
                                           AppSurfaceId app_surface_id,
                                           WindowSessionBounds bounds,
                                           bool chrome_visible,
                                           bool visible,
                                           bool focused,
                                           bool active)
{
    const bool open = visible;
    return {
        id,
        app_surface_id,
        bounds,
        open ? WindowSessionState::Open : WindowSessionState::Hidden,
        open && focused,
        open && active,
        chrome_visible,
        WindowSessionRole::Terminal,
    };
}

void WindowSessionRegistry::clear()
{
    for (size_t index = 0; index < count_; ++index)
    {
        sessions_[index] = {};
    }
    count_ = 0;
}

bool WindowSessionRegistry::register_session(WindowSession session)
{
    if (!session.valid() || session.closed() || find(session.id) != nullptr ||
        find_by_app_surface(session.app_surface_id) != nullptr || count_ >= kMaxWindowSessions)
    {
        return false;
    }

    sessions_[count_++] = session;
    return true;
}

bool WindowSessionRegistry::restore_session(WindowSession session)
{
    if (!session.valid())
    {
        return false;
    }

    WindowSession * target = find_mutable(session.id);
    if (target == nullptr || target->app_surface_id != session.app_surface_id)
    {
        return false;
    }

    *target = session;
    return true;
}

bool WindowSessionRegistry::set_bounds(WindowSessionId id, WindowSessionBounds bounds)
{
    if (!bounds.valid())
    {
        return false;
    }

    WindowSession * target = find_mutable(id);
    if (target == nullptr || target->closed())
    {
        return false;
    }

    target->bounds = bounds;
    return true;
}

bool WindowSessionRegistry::set_visible(WindowSessionId id, bool visible)
{
    WindowSession * target = find_mutable(id);
    if (target == nullptr || target->closed())
    {
        return false;
    }

    target->state = visible ? WindowSessionState::Open : WindowSessionState::Hidden;
    if (!visible)
    {
        target->focused = false;
        target->active = false;
    }
    return true;
}

bool WindowSessionRegistry::set_focused(WindowSessionId id)
{
    WindowSession * target = find_mutable(id);
    if (target == nullptr || !target->open())
    {
        return false;
    }

    clear_focus();
    target->focused = true;
    return true;
}

bool WindowSessionRegistry::set_active(WindowSessionId id)
{
    WindowSession * target = find_mutable(id);
    if (target == nullptr || !target->open())
    {
        return false;
    }

    clear_active();
    target->active = true;
    return true;
}

bool WindowSessionRegistry::close_session(WindowSessionId id)
{
    WindowSession * target = find_mutable(id);
    if (target == nullptr || target->closed())
    {
        return false;
    }

    target->state = WindowSessionState::Closed;
    target->focused = false;
    target->active = false;
    return true;
}

void WindowSessionRegistry::clear_focus()
{
    for (size_t index = 0; index < count_; ++index)
    {
        sessions_[index].focused = false;
    }
}

void WindowSessionRegistry::clear_active()
{
    for (size_t index = 0; index < count_; ++index)
    {
        sessions_[index].active = false;
    }
}

const WindowSession * WindowSessionRegistry::find(WindowSessionId id) const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (sessions_[index].id == id)
        {
            return &sessions_[index];
        }
    }
    return nullptr;
}

const WindowSession * WindowSessionRegistry::find_by_app_surface(AppSurfaceId id) const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (sessions_[index].app_surface_id == id)
        {
            return &sessions_[index];
        }
    }
    return nullptr;
}

const WindowSession * WindowSessionRegistry::focused_session() const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (sessions_[index].focused && sessions_[index].open())
        {
            return &sessions_[index];
        }
    }
    return nullptr;
}

const WindowSession * WindowSessionRegistry::active_session() const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (sessions_[index].active && sessions_[index].open())
        {
            return &sessions_[index];
        }
    }
    return nullptr;
}

const WindowSession * WindowSessionRegistry::at(size_t index) const
{
    return index < count_ ? &sessions_[index] : nullptr;
}

WindowSession * WindowSessionRegistry::find_mutable(WindowSessionId id)
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (sessions_[index].id == id)
        {
            return &sessions_[index];
        }
    }
    return nullptr;
}

void WindowSessionHost::reset(WindowSessionRegistry & sessions, AppSurfaceHost & app_host)
{
    sessions_ = &sessions;
    app_host_ = &app_host;
}

bool WindowSessionHost::ready() const
{
    return sessions_ != nullptr && app_host_ != nullptr && app_host_->ready();
}

const WindowSession * WindowSessionHost::find(WindowSessionId id) const
{
    return ready() ? sessions_->find(id) : nullptr;
}

bool WindowSessionHost::restore_session(WindowSession previous, AppSurface previous_app_surface)
{
    return ready() && app_host_->restore_surface(previous_app_surface) &&
           sessions_->restore_session(previous);
}

bool WindowSessionHost::update_bounds(WindowSessionId id,
                                      WindowSessionBounds bounds,
                                      WindowSessionMutation & mutation)
{
    mutation = {};
    const WindowSession * current = find(id);
    if (current == nullptr || current->closed() || !bounds.valid())
    {
        return false;
    }

    const WindowSession previous = *current;
    AppSurfaceMutation app_mutation;
    if (!app_host_->resize_surface(previous.app_surface_id, bounds.outer, app_mutation))
    {
        return false;
    }

    if (!sessions_->set_bounds(id, bounds))
    {
        if (!restore_session(previous, app_mutation.previous))
        {
            return false;
        }
        return false;
    }

    const WindowSession * updated = find(id);
    if (updated == nullptr)
    {
        if (!restore_session(previous, app_mutation.previous))
        {
            return false;
        }
        return false;
    }

    mutation = {previous, *updated, app_mutation, app_mutation.repaint_bounds};
    return true;
}

bool WindowSessionHost::move_session(WindowSessionId id,
                                     WindowSessionBounds bounds,
                                     WindowSessionMutation & mutation)
{
    return update_bounds(id, bounds, mutation);
}

bool WindowSessionHost::resize_session(WindowSessionId id,
                                       WindowSessionBounds bounds,
                                       WindowSessionMutation & mutation)
{
    return update_bounds(id, bounds, mutation);
}

bool WindowSessionHost::set_visible(WindowSessionId id, bool visible, WindowSessionMutation & mutation)
{
    mutation = {};
    const WindowSession * current = find(id);
    if (current == nullptr || current->closed())
    {
        return false;
    }

    const WindowSession previous = *current;
    AppSurfaceMutation app_mutation;
    if (!app_host_->set_visible(previous.app_surface_id, visible, app_mutation))
    {
        return false;
    }

    if (!sessions_->set_visible(id, visible))
    {
        if (!restore_session(previous, app_mutation.previous))
        {
            return false;
        }
        return false;
    }

    const WindowSession * updated = find(id);
    if (updated == nullptr)
    {
        if (!restore_session(previous, app_mutation.previous))
        {
            return false;
        }
        return false;
    }

    mutation = {previous, *updated, app_mutation, app_mutation.repaint_bounds};
    return true;
}

bool WindowSessionHost::clear_focus(WindowSessionId id, WindowSessionMutation & mutation)
{
    mutation = {};
    const WindowSession * current = find(id);
    if (current == nullptr || current->closed())
    {
        return false;
    }

    const WindowSession previous = *current;
    AppSurfaceMutation app_mutation;
    if (!app_host_->clear_focus(previous.app_surface_id, app_mutation))
    {
        return false;
    }

    sessions_->clear_focus();
    sessions_->clear_active();

    const WindowSession * updated = find(id);
    if (updated == nullptr)
    {
        if (!restore_session(previous, app_mutation.previous))
        {
            return false;
        }
        return false;
    }

    mutation = {previous, *updated, app_mutation, app_mutation.repaint_bounds};
    return true;
}

bool WindowSessionHost::focus_session(WindowSessionId id, WindowSessionMutation & mutation)
{
    mutation = {};
    const WindowSession * current = find(id);
    if (current == nullptr || !current->open())
    {
        return false;
    }

    const WindowSession previous = *current;
    AppSurfaceMutation app_mutation;
    if (!app_host_->focus_surface(previous.app_surface_id, app_mutation))
    {
        return false;
    }

    if (!sessions_->set_focused(id))
    {
        if (!restore_session(previous, app_mutation.previous))
        {
            return false;
        }
        return false;
    }

    const WindowSession * updated = find(id);
    if (updated == nullptr)
    {
        if (!restore_session(previous, app_mutation.previous))
        {
            return false;
        }
        return false;
    }

    mutation = {previous, *updated, app_mutation, app_mutation.repaint_bounds};
    return true;
}

bool WindowSessionHost::close_session(WindowSessionId id, WindowSessionMutation & mutation)
{
    mutation = {};
    const WindowSession * current = find(id);
    if (current == nullptr || current->closed())
    {
        return false;
    }

    const WindowSession previous = *current;
    AppSurfaceMutation app_mutation;
    if (!app_host_->close_surface(previous.app_surface_id, app_mutation))
    {
        return false;
    }

    if (!sessions_->close_session(id))
    {
        if (!restore_session(previous, app_mutation.previous))
        {
            return false;
        }
        return false;
    }

    const WindowSession * updated = find(id);
    if (updated == nullptr)
    {
        if (!restore_session(previous, app_mutation.previous))
        {
            return false;
        }
        return false;
    }

    mutation = {previous, *updated, app_mutation, app_mutation.repaint_bounds};
    return true;
}

} // namespace kernel::display
