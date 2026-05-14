#pragma once

namespace kernel::arch::x86_64
{

void init_irqs();
void enable_interrupts();
void disable_interrupts();

} // namespace kernel::arch::x86_64
