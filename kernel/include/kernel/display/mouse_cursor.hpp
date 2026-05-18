#pragma once

#include <stdint.h>

#include "kernel/display/pointer_cursor_shape.hpp"

namespace kernel::display::mouse_cursor
{

struct Position
{
    uint64_t x = 0;
    uint64_t y = 0;
};

[[nodiscard]] bool init();
Position position();
void show();
void move_by(int16_t delta_x, int16_t delta_y);
void set_shape(PointerCursorShape shape);

} // namespace kernel::display::mouse_cursor
