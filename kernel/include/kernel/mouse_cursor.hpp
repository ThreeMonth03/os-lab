#pragma once

#include <stdint.h>

namespace kernel::mouse_cursor {

[[nodiscard]] bool init();
[[nodiscard]] bool ready();
void show();
void hide();
void move_by(int16_t delta_x, int16_t delta_y);

} // namespace kernel::mouse_cursor
