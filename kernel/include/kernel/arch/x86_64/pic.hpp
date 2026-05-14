#pragma once

#include <stdint.h>

namespace kernel::arch::x86_64::pic
{

inline constexpr uint8_t irq_count = 16;
inline constexpr uint8_t vector_base = 32;

void remap();
void mask_all();
void mask(uint8_t irq_line);
void unmask(uint8_t irq_line);
void send_eoi(uint8_t irq_line);

} // namespace kernel::arch::x86_64::pic
