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

bool g_left_shift_pressed = false;
bool g_right_shift_pressed = false;

inline uint8_t inb(uint16_t port) {
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

bool output_ready() { return (inb(kStatusPort) & kStatusOutputReady) != 0; }

bool shift_active() { return g_left_shift_pressed || g_right_shift_pressed; }

char decode_character(uint8_t scancode, bool shift) {
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
        return shift ? 'Q' : 'q';
    case 0x11:
        return shift ? 'W' : 'w';
    case 0x12:
        return shift ? 'E' : 'e';
    case 0x13:
        return shift ? 'R' : 'r';
    case 0x14:
        return shift ? 'T' : 't';
    case 0x15:
        return shift ? 'Y' : 'y';
    case 0x16:
        return shift ? 'U' : 'u';
    case 0x17:
        return shift ? 'I' : 'i';
    case 0x18:
        return shift ? 'O' : 'o';
    case 0x19:
        return shift ? 'P' : 'p';
    case 0x1a:
        return shift ? '{' : '[';
    case 0x1b:
        return shift ? '}' : ']';
    case 0x1e:
        return shift ? 'A' : 'a';
    case 0x1f:
        return shift ? 'S' : 's';
    case 0x20:
        return shift ? 'D' : 'd';
    case 0x21:
        return shift ? 'F' : 'f';
    case 0x22:
        return shift ? 'G' : 'g';
    case 0x23:
        return shift ? 'H' : 'h';
    case 0x24:
        return shift ? 'J' : 'j';
    case 0x25:
        return shift ? 'K' : 'k';
    case 0x26:
        return shift ? 'L' : 'l';
    case 0x27:
        return shift ? ':' : ';';
    case 0x28:
        return shift ? '"' : '\'';
    case 0x29:
        return shift ? '~' : '`';
    case 0x2b:
        return shift ? '|' : '\\';
    case 0x2c:
        return shift ? 'Z' : 'z';
    case 0x2d:
        return shift ? 'X' : 'x';
    case 0x2e:
        return shift ? 'C' : 'c';
    case 0x2f:
        return shift ? 'V' : 'v';
    case 0x30:
        return shift ? 'B' : 'b';
    case 0x31:
        return shift ? 'N' : 'n';
    case 0x32:
        return shift ? 'M' : 'm';
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

kernel::keyboard::Key key_for_scancode(uint8_t scancode) {
    switch (scancode) {
    case 0x0e:
        return kernel::keyboard::Key::Backspace;
    case 0x1c:
        return kernel::keyboard::Key::Enter;
    case kLeftShift:
    case kRightShift:
        return kernel::keyboard::Key::Shift;
    default:
        return kernel::keyboard::Key::Unknown;
    }
}

} // namespace

namespace kernel::keyboard {

bool poll_key(KeyEvent& event) {
    event = {};

    if (!output_ready()) {
        return false;
    }

    const uint8_t raw_scancode = inb(kDataPort);
    if (raw_scancode == kExtendedPrefix || raw_scancode == kPausePrefix) {
        return false;
    }

    const bool pressed = (raw_scancode & kReleaseMask) == 0;
    const uint8_t scancode = raw_scancode & ~kReleaseMask;

    if (scancode == kLeftShift) {
        g_left_shift_pressed = pressed;
    } else if (scancode == kRightShift) {
        g_right_shift_pressed = pressed;
    }

    event.pressed = pressed;
    event.shift = shift_active();
    event.key = key_for_scancode(scancode);

    if (!pressed) {
        return event.key == Key::Shift;
    }

    const char character = decode_character(scancode, event.shift);
    if (character != '\0') {
        event.key = Key::Character;
        event.character = character;
    }

    return event.key != Key::Unknown;
}

} // namespace kernel::keyboard
