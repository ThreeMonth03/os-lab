#pragma once

#include "kernel/input/input_types.hpp"
#include "kernel/input/mouse.hpp"

namespace kernel::drivers::mouse
{

[[nodiscard]] bool init();
input::DeviceMode input_mode();
void handle_irq();
[[nodiscard]] bool poll(kernel::input::mouse::MouseEvent & event);

} // namespace kernel::drivers::mouse
