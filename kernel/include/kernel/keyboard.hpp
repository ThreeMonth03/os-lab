#pragma once

namespace kernel::keyboard {

enum class Key {
    Unknown,
    Character,
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

struct KeyEvent {
    Key key = Key::Unknown;
    char character = '\0';
    bool pressed = false;
    bool shift = false;
    bool control = false;
    bool alt = false;
    bool caps_lock = false;
    bool extended = false;
};

[[nodiscard]] bool poll_key(KeyEvent& event);

} // namespace kernel::keyboard
