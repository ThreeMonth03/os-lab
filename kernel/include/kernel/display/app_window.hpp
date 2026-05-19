#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/app_surface.hpp"
#include "kernel/display/window_session.hpp"

namespace kernel::display
{

using AppWindowId = uint16_t;

inline constexpr AppWindowId kInvalidAppWindowId = 0;
inline constexpr AppWindowId kTerminalAppWindowId = 1;
inline constexpr AppWindowId kDummyDebugAppWindowId = 2;
inline constexpr size_t kMaxAppWindows = kMaxWindowSessions;

enum class AppWindowKind
{
    None,
    Terminal,
    DummyDebugApp,
    ExternalApp,
};

enum class AppWindowTitle
{
    None,
    Terminal,
    DummyDebugApp,
    ExternalApp,
};

struct AppWindowClient
{
    AppWindowKind kind = AppWindowKind::None;
    AppSurfaceId surface_id = kInvalidAppSurfaceId;

    [[nodiscard]] bool valid() const;
};

struct AppWindow
{
    AppWindowId id = kInvalidAppWindowId;
    WindowSessionId session_id = kInvalidWindowSessionId;
    AppWindowClient client;
    WindowSessionBounds bounds;
    WindowSessionState state = WindowSessionState::Closed;
    bool focused = false;
    bool active = false;
    bool chrome_visible = false;
    AppWindowTitle title = AppWindowTitle::None;

    [[nodiscard]] bool valid() const;
    [[nodiscard]] bool open() const { return state == WindowSessionState::Open; }
    [[nodiscard]] bool hidden() const { return state == WindowSessionState::Hidden; }
    [[nodiscard]] bool closed() const { return state == WindowSessionState::Closed; }
    [[nodiscard]] bool visible() const { return open(); }
};

[[nodiscard]] AppWindowKind app_window_kind_for(WindowSessionRole role);
[[nodiscard]] AppWindowTitle app_window_title_for(AppWindowKind kind);
[[nodiscard]] AppWindowId app_window_id_for(WindowSessionId session_id);
[[nodiscard]] AppWindow app_window_for_session(WindowSession session);

class AppWindowRegistry
{
public:
    AppWindowRegistry() = default;

    void clear();
    [[nodiscard]] bool register_window(AppWindow window);
    [[nodiscard]] bool restore_window(AppWindow window);
    [[nodiscard]] bool sync_session(WindowSession session);
    [[nodiscard]] bool set_visible(AppWindowId id, bool visible);
    [[nodiscard]] bool close_window(AppWindowId id);

    [[nodiscard]] const AppWindow * find(AppWindowId id) const;
    [[nodiscard]] const AppWindow * find_by_session(WindowSessionId id) const;
    [[nodiscard]] const AppWindow * find_by_surface(AppSurfaceId id) const;
    [[nodiscard]] const AppWindow * at(size_t index) const;

    [[nodiscard]] size_t size() const { return count_; }
    [[nodiscard]] size_t capacity() const { return kMaxAppWindows; }

private:
    [[nodiscard]] AppWindow * find_mutable(AppWindowId id);

    AppWindow windows_[kMaxAppWindows] = {};
    size_t count_ = 0;
};

} // namespace kernel::display
