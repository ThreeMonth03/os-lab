#include "kernel/display/app_window.hpp"

namespace kernel::display
{

bool AppWindowClient::valid() const
{
    return kind != AppWindowKind::None && surface_id != kInvalidAppSurfaceId;
}

bool AppWindow::valid() const
{
    return id != kInvalidAppWindowId && session_id != kInvalidWindowSessionId &&
           client.valid() && bounds.valid() && title != AppWindowTitle::None;
}

AppWindowKind app_window_kind_for(WindowSessionRole role)
{
    switch (role)
    {
    case WindowSessionRole::Terminal:
        return AppWindowKind::Terminal;
    case WindowSessionRole::DummyDebugApp:
        return AppWindowKind::DummyDebugApp;
    }
    return AppWindowKind::None;
}

AppWindowTitle app_window_title_for(AppWindowKind kind)
{
    switch (kind)
    {
    case AppWindowKind::Terminal:
        return AppWindowTitle::Terminal;
    case AppWindowKind::DummyDebugApp:
        return AppWindowTitle::DummyDebugApp;
    case AppWindowKind::ExternalApp:
        return AppWindowTitle::ExternalApp;
    case AppWindowKind::None:
        return AppWindowTitle::None;
    }
    return AppWindowTitle::None;
}

AppWindowId app_window_id_for(WindowSessionId session_id)
{
    switch (session_id)
    {
    case kTerminalWindowSessionId:
        return kTerminalAppWindowId;
    case kDummyDebugWindowSessionId:
        return kDummyDebugAppWindowId;
    case kInvalidWindowSessionId:
        return kInvalidAppWindowId;
    default:
        return static_cast<AppWindowId>(session_id);
    }
}

AppWindow app_window_for_session(WindowSession session)
{
    const AppWindowKind kind = app_window_kind_for(session.role);
    return {
        app_window_id_for(session.id),
        session.id,
        {kind, session.app_surface_id},
        session.bounds,
        session.state,
        session.visible() && session.focused,
        session.visible() && session.active,
        session.chrome_visible,
        app_window_title_for(kind),
    };
}

void AppWindowRegistry::clear()
{
    for (size_t index = 0; index < count_; ++index)
    {
        windows_[index] = {};
    }
    count_ = 0;
}

bool AppWindowRegistry::register_window(AppWindow window)
{
    if (!window.valid() || window.closed() || find(window.id) != nullptr ||
        find_by_session(window.session_id) != nullptr ||
        find_by_surface(window.client.surface_id) != nullptr || count_ >= kMaxAppWindows)
    {
        return false;
    }

    windows_[count_++] = window;
    return true;
}

bool AppWindowRegistry::restore_window(AppWindow window)
{
    if (!window.valid())
    {
        return false;
    }

    AppWindow * target = find_mutable(window.id);
    if (target == nullptr || target->session_id != window.session_id ||
        target->client.surface_id != window.client.surface_id)
    {
        return false;
    }

    *target = window;
    return true;
}

bool AppWindowRegistry::sync_session(WindowSession session)
{
    return restore_window(app_window_for_session(session));
}

bool AppWindowRegistry::set_visible(AppWindowId id, bool visible)
{
    AppWindow * target = find_mutable(id);
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

bool AppWindowRegistry::close_window(AppWindowId id)
{
    AppWindow * target = find_mutable(id);
    if (target == nullptr || target->closed())
    {
        return false;
    }

    target->state = WindowSessionState::Closed;
    target->focused = false;
    target->active = false;
    return true;
}

const AppWindow * AppWindowRegistry::find(AppWindowId id) const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (windows_[index].id == id)
        {
            return &windows_[index];
        }
    }
    return nullptr;
}

const AppWindow * AppWindowRegistry::find_by_session(WindowSessionId id) const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (windows_[index].session_id == id)
        {
            return &windows_[index];
        }
    }
    return nullptr;
}

const AppWindow * AppWindowRegistry::find_by_surface(AppSurfaceId id) const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (windows_[index].client.surface_id == id)
        {
            return &windows_[index];
        }
    }
    return nullptr;
}

const AppWindow * AppWindowRegistry::at(size_t index) const
{
    return index < count_ ? &windows_[index] : nullptr;
}

AppWindow * AppWindowRegistry::find_mutable(AppWindowId id)
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (windows_[index].id == id)
        {
            return &windows_[index];
        }
    }
    return nullptr;
}

} // namespace kernel::display
