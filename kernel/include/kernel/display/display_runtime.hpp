#pragma once

#include <stdint.h>

#include "kernel/display/hit_test.hpp"

namespace kernel::display::runtime
{

[[nodiscard]] HitTestResult pointer_target();

void refresh_debug_overlay_if_due();
void update_pointer_target(uint64_t x, uint64_t y);

} // namespace kernel::display::runtime
