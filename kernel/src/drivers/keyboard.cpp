#include "kernel/keyboard.hpp"

#include <stdint.h>

namespace {

constexpr uint16_t kDataPort = 0x60;
constexpr uint16_t kStatusPort = 0x64;
constexpr uint8_t kStatusOutputReady = 0x01;
constexpr uint8_t kReleaseMask = 0x80;
constexpr uint8_t kExtendedPrefix = 0xe0;
constexpr uint8_t kPausePrefix = 0xe1;
constexpr uint8_t kLeftShift = 0x2a;
constexpr uint8_t kRightShift = 0x36;
constexpr uint8_t kControl = 0x1d;
constexpr uint8_t kAlt = 0x38;
constexpr uint8_t kCapsLock = 0x3a;

bool g_left_shift_pressed = false;
bool g_right_shift_pressed = false;
bool g_left_control_pressed = false;
bool g_right_control_pressed = false;
bool g_left_alt_pressed = false;
bool g_right_alt_pressed = false;
bool g_caps_lock_enabled = false;
bool g_extended_scancode = false;

inline uint8_t inb(uint16_t port) {
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

bool output_ready() { return (inb(kStatusPort) & kStatusOutputReady) != 0; }

bool shift_active() { return g_left_shift_pressed || g_right_shift_pressed; }
bool control_active() { return g_left_control_pressed || g_right_control_pressed; }
bool alt_active() { return g_left_alt_pressed || g_right_alt_pressed; }

char letter(char lowercase, bool shift, bool caps_lock) {
    const bool uppercase = shift != caps_lock;
    if (uppercase) {
        return static_cast<char>(lowercase - 'a' + 'A');
    }

    return lowercase;
}

char decode_character(uint8_t scancode, bool shift, bool caps_lock) {
    switch (scancode) {
    case 0x02:
        return shift ? '!' : '1';
    case 0x03:
        return shift ? '@' : '2';
    case 0x04:
        return shift ? '#' : '3';
    case 0x05:
        return shift ? '$' : '4';
    case 0x06:
        return shift ? '%' : '5';
    case 0x07:
        return shift ? '^' : '6';
    case 0x08:
        return shift ? '&' : '7';
    case 0x09:
        return shift ? '*' : '8';
    case 0x0a:
        return shift ? '(' : '9';
    case 0x0b:
        return shift ? ')' : '0';
    case 0x0c:
        return shift ? '_' : '-';
    case 0x0d:
        return shift ? '+' : '=';
    case 0x10:
        return letter('q', shift, caps_lock);
    case 0x11:
        return letter('w', shift, caps_lock);
    case 0x12:
        return letter('e', shift, caps_lock);
    case 0x13:
        return letter('r', shift, caps_lock);
    case 0x14:
        return letter('t', shift, caps_lock);
    case 0x15:
        return letter('y', shift, caps_lock);
    case 0x16:
        return letter('u', shift, caps_lock);
    case 0x17:
        return letter('i', shift, caps_lock);
    case 0x18:
        return letter('o', shift, caps_lock);
    case 0x19:
        return letter('p', shift, caps_lock);
    case 0x1a:
        return shift ? '{' : '[';
    case 0x1b:
        return shift ? '}' : ']';
    case 0x1e:
        return letter('a', shift, caps_lock);
    case 0x1f:
        return letter('s', shift, caps_lock);
    case 0x20:
        return letter('d', shift, caps_lock);
    case 0x21:
        return letter('f', shift, caps_lock);
    case 0x22:
        return letter('g', shift, caps_lock);
    case 0x23:
        return letter('h', shift, caps_lock);
    case 0x24:
        return letter('j', shift, caps_lock);
    case 0x25:
        return letter('k', shift, caps_lock);
    case 0x26:
        return letter('l', shift, caps_lock);
    case 0x27:
        return shift ? ':' : ';';
    case 0x28:
        return shift ? '"' : '\'';
    case 0x29:
        return shift ? '~' : '`';
    case 0x2b:
        return shift ? '|' : '\\';
    case 0x2c:
        return letter('z', shift, caps_lock);
    case 0x2d:
        return letter('x', shift, caps_lock);
    case 0x2e:
        return letter('c', shift, caps_lock);
    case 0x2f:
        return letter('v', shift, caps_lock);
    case 0x30:
        return letter('b', shift, caps_lock);
    case 0x31:
        return letter('n', shift, caps_lock);
    case 0x32:
        return letter('m', shift, caps_lock);
    case 0x33:
        return shift ? '<' : ',';
    case 0x34:
        return shift ? '>' : '.';
    case 0x35:
        return shift ? '?' : '/';
    case 0x39:
        return ' ';
    default:
        return '\0';
    }
}

kernel::keyboard::Key key_for_scancode(uint8_t scancode, bool extended) {
    if (extended) {
        switch (scancode) {
        case 0x4b:
            return kernel::keyboard::Key::LeftArrow;
        case 0x4d:
            return kernel::keyboard::Key::RightArrow;
        case 0x53:
            return kernel::keyboard::Key::Delete;
        case kControl:
            return kernel::keyboard::Key::Control;
        case kAlt:
            return kernel::keyboard::Key::Alt;
        default:
            return kernel::keyboard::Key::Unknown;
        }
    }

    switch (scancode) {
    case 0x0e:
        return kernel::keyboard::Key::Backspace;
    case 0x1c:
        return kernel::keyboard::Key::Enter;
    case kLeftShift:
    case kRightShift:
        return kernel::keyboard::Key::Shift;
    case kControl:
        return kernel::keyboard::Key::Control;
    case kAlt:
        return kernel::keyboard::Key::Alt;
    case kCapsLock:
        return kernel::keyboard::Key::CapsLock;
    default:
        return kernel::keyboard::Key::Unknown;
    }
}

void update_modifier_state(uint8_t scancode, bool extended, bool pressed) {
    if (!extended && scancode == kLeftShift) {
        g_left_shift_pressed = pressed;
    } else if (!extended && scancode == kRightShift) {
        g_right_shift_pressed = pressed;
    } else if (!extended && scancode == kControl) {
        g_left_control_pressed = pressed;
    } else if (extended && scancode == kControl) {
        g_right_control_pressed = pressed;
    } else if (!extended && scancode == kAlt) {
        g_left_alt_pressed = pressed;
    } else if (extended && scancode == kAlt) {
        g_right_alt_pressed = pressed;
    } else if (!extended && scancode == kCapsLock && pressed) {
        g_caps_lock_enabled = !g_caps_lock_enabled;
    }
}

void fill_modifiers(kernel::keyboard::KeyEvent& event) {
    event.shift = shift_active();
    event.control = control_active();
    event.alt = alt_active();
    event.caps_lock = g_caps_lock_enabled;
}

} // namespace

namespace kernel::keyboard {

bool poll_key(KeyEvent& event) {
    event = {};

    if (!output_ready()) {
        return false;
    }

    uint8_t raw_scancode = inb(kDataPort);
    if (raw_scancode == kExtendedPrefix) {
        g_extended_scancode = true;
        return false;
    }

    if (raw_scancode == kPausePrefix) {
        g_extended_scancode = false;
        return false;
    }

    const bool extended = g_extended_scancode;
    g_extended_scancode = false;

    const bool pressed = (raw_scancode & kReleaseMask) == 0;
    const uint8_t scancode = raw_scancode & ~kReleaseMask;

    update_modifier_state(scancode, extended, pressed);

    event.pressed = pressed;
    event.extended = extended;
    fill_modifiers(event);
    event.key = key_for_scancode(scancode, extended);

    if (!pressed) {
        return event.key == Key::Shift || event.key == Key::Control || event.key == Key::Alt ||
               event.key == Key::CapsLock;
    }

    const char character =
        extended ? '\0' : decode_character(scancode, event.shift, event.caps_lock);
    if (character != '\0' && !event.control && !event.alt) {
        event.key = Key::Character;
        event.character = character;
    }

    return event.key != Key::Unknown;
}

} // namespace kernel::keyboard
