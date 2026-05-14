#pragma once

#include <stdint.h>

namespace kernel::arch::x86_64
{

void init_exceptions();
void set_interrupt_gate(uint8_t vector, void (*handler)());

} // namespace kernel::arch::x86_64
