#pragma once

#include <stdint.h>

namespace kernel::time::timer
{

inline constexpr uint32_t frequency_hz = 100;

void init();
uint64_t ticks();
void handle_tick();

} // namespace kernel::time::timer
