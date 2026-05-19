#pragma once

#include "kernel/display/window_session.hpp"
#include "kernel/display/window_stack.hpp"

namespace kernel::display
{

struct WindowManagerSnapshot
{
    WindowSessionId focused = kInvalidWindowSessionId;
    WindowSessionId active = kInvalidWindowSessionId;
    WindowSessionId topmost_visible = kInvalidWindowSessionId;
    size_t window_count = 0;
};

struct WindowManagerResult
{
    bool success = false;
    bool changed = false;
    WindowSessionMutation session;
    WindowStack previous_stack;
    WindowStack current_stack;

    [[nodiscard]] bool valid() const;
};

class WindowManager
{
public:
    WindowManager() = default;

    void reset(WindowSessionHost & sessions, WindowStack & stack);

    [[nodiscard]] bool ready() const;
    [[nodiscard]] const WindowSession * find_window(WindowSessionId id) const;
    [[nodiscard]] WindowManagerSnapshot snapshot() const;
    [[nodiscard]] bool selected(WindowSessionId id) const;

    [[nodiscard]] bool show_window(WindowSessionId id, WindowManagerResult & result);
    [[nodiscard]] bool hide_window(WindowSessionId id, WindowManagerResult & result);
    [[nodiscard]] bool clear_focus(WindowSessionId id, WindowManagerResult & result);
    [[nodiscard]] bool focus_window(WindowSessionId id, WindowManagerResult & result);
    [[nodiscard]] bool activate_window(WindowSessionId id, WindowManagerResult & result);
    [[nodiscard]] bool raise_window(WindowSessionId id, WindowManagerResult & result);
    [[nodiscard]] bool focus_activate_raise_window(WindowSessionId id,
                                                   WindowManagerResult & result);
    [[nodiscard]] bool show_focus_activate_raise_window(WindowSessionId id,
                                                        WindowManagerResult & result);
    [[nodiscard]] bool move_window(WindowSessionId id,
                                   WindowSessionBounds bounds,
                                   WindowManagerResult & result);
    [[nodiscard]] bool resize_window(WindowSessionId id,
                                     WindowSessionBounds bounds,
                                     WindowManagerResult & result);
    [[nodiscard]] bool close_window(WindowSessionId id, WindowManagerResult & result);

private:
    enum class StackPolicy
    {
        Sync,
        Focus,
        Activate,
        Raise,
        FocusActivateRaise,
    };

    [[nodiscard]] bool finish_session_mutation(WindowSessionMutation mutation,
                                               StackPolicy policy,
                                               WindowManagerResult & result);
    [[nodiscard]] bool apply_stack_policy(WindowSession & session, StackPolicy policy);
    [[nodiscard]] bool stack_only_mutation(WindowSessionId id,
                                           StackPolicy policy,
                                           WindowManagerResult & result);
    void set_noop(WindowManagerResult & result) const;
    [[nodiscard]] bool rollback(WindowSession previous,
                                AppSurface previous_app,
                                WindowStack previous_stack);

    WindowSessionHost * sessions_ = nullptr;
    WindowStack * stack_ = nullptr;
};

} // namespace kernel::display
