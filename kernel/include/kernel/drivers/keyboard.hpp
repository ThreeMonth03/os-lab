#pragma once

#include "kernel/input/input_types.hpp"
#include "kernel/input/keyboard.hpp"

namespace kernel::drivers::keyboard
{

[[nodiscard]] bool init_irq();
input::DeviceMode input_mode();
void handle_irq();
[[nodiscard]] bool poll_key(kernel::input::keyboard::KeyEvent & event);

} // namespace kernel::drivers::keyboard
