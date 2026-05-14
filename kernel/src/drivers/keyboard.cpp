#include "kernel/keyboard.hpp"

#include <stdint.h>

#include "kernel/keyboard_decoder.hpp"

namespace {

constexpr uint16_t kDataPort = 0x60;
constexpr uint16_t kStatusPort = 0x64;
constexpr uint8_t kStatusOutputReady = 0x01;

kernel::keyboard::KeyboardDecoder g_decoder;

inline uint8_t inb(uint16_t port) {
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

bool output_ready() { return (inb(kStatusPort) & kStatusOutputReady) != 0; }

} // namespace

namespace kernel::keyboard {

bool poll_key(KeyEvent& event) {
    event = {};

    if (!output_ready()) {
        return false;
    }

    return g_decoder.decode(inb(kDataPort), event);
}

} // namespace kernel::keyboard
