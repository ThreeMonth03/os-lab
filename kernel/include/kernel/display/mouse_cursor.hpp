#pragma once

#include <stdint.h>

namespace kernel::display::mouse_cursor
{

struct Position
{
    uint64_t x = 0;
    uint64_t y = 0;
};

[[nodiscard]] bool init();
bool ready();
Position position();
void show();
void hide();
void move_by(int16_t delta_x, int16_t delta_y);

} // namespace kernel::display::mouse_cursor
