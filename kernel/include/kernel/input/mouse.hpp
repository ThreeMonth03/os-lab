#pragma once

#include <stdint.h>

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

[[nodiscard]] bool init();
[[nodiscard]] bool ready();
[[nodiscard]] bool poll(MouseEvent & event);

} // namespace kernel::mouse
