#pragma once

#include <stdint.h>

namespace kernel::drivers::ps2 {

enum class Device {
    Keyboard,
    Mouse,
};

[[nodiscard]] bool read_keyboard_data(uint8_t& data);
[[nodiscard]] bool read_mouse_data(uint8_t& data);
[[nodiscard]] bool read_data(uint8_t& data, Device& device);
[[nodiscard]] bool write_command(uint8_t command);
[[nodiscard]] bool write_data(uint8_t data);
[[nodiscard]] bool write_mouse_data(uint8_t data);
void flush_output();

} // namespace kernel::drivers::ps2
