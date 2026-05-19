#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/app_surface.hpp"
#include "kernel/display/app_surface_host.hpp"
#include "kernel/display/composited_surface.hpp"
#include "kernel/display/display.hpp"

namespace kernel::display
{

using WindowSessionId = uint16_t;

inline constexpr WindowSessionId kInvalidWindowSessionId = 0;
inline constexpr WindowSessionId kTerminalWindowSessionId = 1;
inline constexpr size_t kMaxWindowSessions = 4;

enum class WindowSessionState
{
    Closed,
    Hidden,
    Open,
};

enum class WindowSessionRole
{
    Terminal,
};

struct WindowSessionBounds
{
    Rect outer;
    Rect client;

    bool valid() const;
};

struct WindowSession
{
    WindowSessionId id = kInvalidWindowSessionId;
    AppSurfaceId app_surface_id = kInvalidAppSurfaceId;
    WindowSessionBounds bounds;
    WindowSessionState state = WindowSessionState::Closed;
    bool focused = false;
    bool active = false;
    bool chrome_visible = false;
    WindowSessionRole role = WindowSessionRole::Terminal;

    bool valid() const;
    bool open() const { return state == WindowSessionState::Open; }
    bool hidden() const { return state == WindowSessionState::Hidden; }
    bool closed() const { return state == WindowSessionState::Closed; }
    bool visible() const { return open(); }
    AppSurface app_surface() const;
    CompositedSurfaceDescriptor composited_surface() const;
};

[[nodiscard]] AppSurface app_surface_for_window_session(WindowSession session);
[[nodiscard]] CompositedSurfaceDescriptor app_composited_surface_for_window_session(
    WindowSession session);
[[nodiscard]] CompositedSurfaceDescriptor retained_app_composited_surface_for_window_session(
    WindowSession session,
    Rect retained_bounds);
[[nodiscard]] SurfaceDescriptor app_display_target_for_window_session(WindowSession session);

WindowSession make_terminal_window_session(WindowSessionId id,
                                           AppSurfaceId app_surface_id,
                                           WindowSessionBounds bounds,
                                           bool chrome_visible,
                                           bool visible = true,
                                           bool focused = false,
                                           bool active = false);

class WindowSessionRegistry
{
public:
    WindowSessionRegistry() = default;

    void clear();
    bool register_session(WindowSession session);
    bool restore_session(WindowSession session);
    bool set_bounds(WindowSessionId id, WindowSessionBounds bounds);
    bool set_visible(WindowSessionId id, bool visible);
    bool set_focused(WindowSessionId id);
    bool set_active(WindowSessionId id);
    bool close_session(WindowSessionId id);
    void clear_focus();
    void clear_active();

    const WindowSession * find(WindowSessionId id) const;
    const WindowSession * find_by_app_surface(AppSurfaceId id) const;
    const WindowSession * focused_session() const;
    const WindowSession * active_session() const;
    const WindowSession * at(size_t index) const;

    size_t size() const { return count_; }
    size_t capacity() const { return kMaxWindowSessions; }

private:
    WindowSession * find_mutable(WindowSessionId id);

    WindowSession sessions_[kMaxWindowSessions] = {};
    size_t count_ = 0;
};

struct WindowSessionMutation
{
    WindowSession previous;
    WindowSession current;
    AppSurfaceMutation app_surface;
    Rect repaint_bounds;

    [[nodiscard]] bool valid() const;
};

class WindowSessionHost
{
public:
    WindowSessionHost() = default;

    void reset(WindowSessionRegistry & sessions, AppSurfaceHost & app_host);

    [[nodiscard]] bool ready() const;
    [[nodiscard]] const WindowSession * find(WindowSessionId id) const;
    [[nodiscard]] bool restore_session(WindowSession previous, AppSurface previous_app_surface);
    [[nodiscard]] bool move_session(WindowSessionId id,
                                    WindowSessionBounds bounds,
                                    WindowSessionMutation & mutation);
    [[nodiscard]] bool resize_session(WindowSessionId id,
                                      WindowSessionBounds bounds,
                                      WindowSessionMutation & mutation);
    [[nodiscard]] bool set_visible(WindowSessionId id, bool visible, WindowSessionMutation & mutation);
    [[nodiscard]] bool clear_focus(WindowSessionId id, WindowSessionMutation & mutation);
    [[nodiscard]] bool focus_session(WindowSessionId id, WindowSessionMutation & mutation);
    [[nodiscard]] bool close_session(WindowSessionId id, WindowSessionMutation & mutation);

private:
    [[nodiscard]] bool update_bounds(WindowSessionId id,
                                     WindowSessionBounds bounds,
                                     WindowSessionMutation & mutation);

    WindowSessionRegistry * sessions_ = nullptr;
    AppSurfaceHost * app_host_ = nullptr;
};

} // namespace kernel::display
