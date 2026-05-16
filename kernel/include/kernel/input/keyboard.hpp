#pragma once

namespace kernel::input::keyboard
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

} // namespace kernel::input::keyboard
