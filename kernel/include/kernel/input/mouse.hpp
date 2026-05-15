#pragma once

#include <stdint.h>

namespace kernel::mouse
{

enum class InputMode
{
    PollingFallback,
    Irq,
};

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

[[nodiscard]] bool init();
[[nodiscard]] bool ready();
[[nodiscard]] InputMode input_mode();
void handle_irq();
[[nodiscard]] bool poll(MouseEvent & event);

} // namespace kernel::mouse
