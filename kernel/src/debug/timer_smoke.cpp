#include "kernel/debug/timer_smoke.hpp"

#include "kernel/core/halt.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/console/terminal.hpp"
#include "kernel/time/timer.hpp"

#ifndef OS_LAB_TIMER_SMOKE
#define OS_LAB_TIMER_SMOKE 0
#endif

namespace {

#if OS_LAB_TIMER_SMOKE

void write_both(kernel::StringView value) {
    kernel::drivers::serial::write_string(value);
    if (kernel::console::terminal::ready()) {
        kernel::console::terminal::write_string(value);
    }
}

void write_both_line(kernel::StringView value) {
    kernel::drivers::serial::write_line(value);
    if (kernel::console::terminal::ready()) {
        kernel::console::terminal::write_line(value);
    }
}

void write_both_decimal(uint64_t value) {
    kernel::drivers::serial::write_decimal(value);
    if (kernel::console::terminal::ready()) {
        kernel::console::terminal::write_decimal(value);
    }
}

#endif

} // namespace

namespace kernel::debug {

void run_timer_smoke() {
#if OS_LAB_TIMER_SMOKE
    const uint64_t target_ticks = kernel::time::timer::frequency_hz / 5;
    const uint64_t start = kernel::time::timer::ticks();

    write_both_line("os-lab: timer smoke waiting for PIT ticks");
    while (kernel::time::timer::ticks() - start < target_ticks) {
        asm volatile("pause");
    }

    write_both("os-lab: timer smoke passed ticks=");
    write_both_decimal(kernel::time::timer::ticks() - start);
    write_both_line("");
    halt_forever();
#endif
}

} // namespace kernel::debug
