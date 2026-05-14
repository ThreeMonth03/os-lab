#include "kernel/keyboard.hpp"

#include <stdint.h>

#include "kernel/keyboard_decoder.hpp"
#include "kernel/ps2_controller.hpp"

namespace {

kernel::keyboard::KeyboardDecoder g_decoder;

} // namespace

namespace kernel::keyboard {

bool poll_key(KeyEvent& event) {
    event = {};

    uint8_t raw_scancode = 0;
    if (!ps2::read_keyboard_data(raw_scancode)) {
        return false;
    }

    return g_decoder.decode(raw_scancode, event);
}

} // namespace kernel::keyboard
