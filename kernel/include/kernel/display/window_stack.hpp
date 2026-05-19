#pragma once

#include <stddef.h>

#include "kernel/display/window_session.hpp"

namespace kernel::display
{

struct WindowStackEntry
{
    WindowSessionId id = kInvalidWindowSessionId;
    WindowSessionState state = WindowSessionState::Closed;
    bool focused = false;
    bool active = false;

    bool valid() const { return id != kInvalidWindowSessionId; }
    bool open() const { return state == WindowSessionState::Open; }
    bool hidden() const { return state == WindowSessionState::Hidden; }
    bool closed() const { return state == WindowSessionState::Closed; }
    bool visible() const { return open(); }
};

class WindowStack
{
public:
    WindowStack() = default;

    void clear();
    [[nodiscard]] bool register_window(WindowSession session);
    [[nodiscard]] bool sync_window(WindowSession session);
    [[nodiscard]] bool set_visible(WindowSessionId id, bool visible);
    [[nodiscard]] bool close_window(WindowSessionId id);
    [[nodiscard]] bool focus_window(WindowSessionId id);
    [[nodiscard]] bool activate_window(WindowSessionId id);
    [[nodiscard]] bool focus_and_activate(WindowSessionId id);
    [[nodiscard]] bool raise_to_front(WindowSessionId id);
    void clear_focus();
    void clear_active();

    [[nodiscard]] bool contains(WindowSessionId id) const;
    [[nodiscard]] const WindowStackEntry * find(WindowSessionId id) const;
    [[nodiscard]] const WindowStackEntry * at(size_t index) const;
    [[nodiscard]] WindowSessionId focused_window() const;
    [[nodiscard]] WindowSessionId active_window() const;
    [[nodiscard]] WindowSessionId topmost_visible_window() const;

    size_t size() const { return count_; }
    size_t capacity() const { return kMaxWindowSessions; }

private:
    [[nodiscard]] WindowStackEntry * find_mutable(WindowSessionId id);
    [[nodiscard]] size_t find_index(WindowSessionId id) const;

    WindowStackEntry entries_[kMaxWindowSessions] = {};
    size_t count_ = 0;
};

} // namespace kernel::display
