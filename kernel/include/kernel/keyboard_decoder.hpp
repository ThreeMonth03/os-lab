#pragma once

#include <stdint.h>

#include "kernel/keyboard.hpp"

namespace kernel::keyboard {

class KeyboardDecoder {
  public:
    [[nodiscard]] bool decode(uint8_t raw_scancode, KeyEvent& event);
    void reset();

  private:
    bool left_shift_pressed_ = false;
    bool right_shift_pressed_ = false;
    bool left_control_pressed_ = false;
    bool right_control_pressed_ = false;
    bool left_alt_pressed_ = false;
    bool right_alt_pressed_ = false;
    bool caps_lock_enabled_ = false;
    bool extended_scancode_ = false;
};

} // namespace kernel::keyboard
