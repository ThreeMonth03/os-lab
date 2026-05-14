#pragma once

#include <stdint.h>

#include "kernel/keyboard.hpp"

namespace kernel::input {

enum class EventKind {
    None,
    Key,
    MouseMove,
};

struct MouseMoveEvent {
    int16_t delta_x = 0;
    int16_t delta_y = 0;
    bool left_button = false;
    bool right_button = false;
    bool middle_button = false;
    bool x_overflow = false;
    bool y_overflow = false;
};

struct Event {
    EventKind kind = EventKind::None;
    keyboard::KeyEvent key = {};
    MouseMoveEvent mouse_move = {};
};

void pump();
[[nodiscard]] bool poll(Event& event);

} // namespace kernel::input
