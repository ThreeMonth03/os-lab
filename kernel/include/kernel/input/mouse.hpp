#pragma once

#include <stdint.h>

#include "kernel/input/input_types.hpp"

namespace kernel::mouse
{

struct MouseEvent
{
    int16_t delta_x = 0;
    int16_t delta_y = 0;
    bool left_button = false;
    bool right_button = false;
    bool middle_button = false;
    bool x_overflow = false;
    bool y_overflow = false;
};

} // namespace kernel::mouse

namespace kernel::drivers::mouse
{

[[nodiscard]] bool init();
input::DeviceMode input_mode();
void handle_irq();
[[nodiscard]] bool poll(kernel::mouse::MouseEvent & event);

} // namespace kernel::drivers::mouse
