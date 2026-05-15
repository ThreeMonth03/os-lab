#pragma once

#include <stdint.h>

#include "kernel/input/keyboard.hpp"

namespace kernel::input
{

enum class EventKind
{
    None,
    Key,
    MouseMove,
};

struct MouseMoveEvent
{
    int16_t delta_x = 0;
    int16_t delta_y = 0;
    bool left_button = false;
    bool right_button = false;
    bool middle_button = false;
    bool x_overflow = false;
    bool y_overflow = false;
};

struct Event
{
    EventKind kind = EventKind::None;
    keyboard::KeyEvent key = {};
    MouseMoveEvent mouse_move = {};
};

struct Stats
{
    uint64_t key_events = 0;
    uint64_t mouse_move_events = 0;
    uint64_t dropped_events = 0;
    uint64_t queued_events = 0;
    uint64_t queue_capacity = 0;
    uint64_t queue_available = 0;
};

void pump();
[[nodiscard]] bool enqueue(const Event & event);
[[nodiscard]] bool poll(Event & event);
[[nodiscard]] Stats stats();

} // namespace kernel::input
