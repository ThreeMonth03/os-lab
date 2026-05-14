#pragma once

namespace kernel::keyboard {

enum class Key {
    Unknown,
    Character,
    Enter,
    Backspace,
    Shift,
};

struct KeyEvent {
    Key key = Key::Unknown;
    char character = '\0';
    bool pressed = false;
    bool shift = false;
};

[[nodiscard]] bool poll_key(KeyEvent& event);

} // namespace kernel::keyboard
