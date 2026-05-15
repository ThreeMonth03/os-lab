#pragma once

#include "kernel/input/input_types.hpp"

namespace kernel::keyboard
{

enum class Key
{
    Unknown,
    Character,
    Tab,
    Enter,
    Backspace,
    Shift,
    Control,
    Alt,
    CapsLock,
    UpArrow,
    DownArrow,
    LeftArrow,
    RightArrow,
    Delete,
};

struct KeyEvent
{
    Key key = Key::Unknown;
    char character = '\0';
    bool pressed = false;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool caps_lock = false;
    bool extended = false;
};

[[nodiscard]] bool init_irq();
[[nodiscard]] input::DeviceMode input_mode();
void handle_irq();
[[nodiscard]] bool poll_key(KeyEvent & event);

} // namespace kernel::keyboard
