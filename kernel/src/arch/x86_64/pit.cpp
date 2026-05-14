#include "kernel/timer.hpp"

#include <stdint.h>

#include "kernel/arch/x86_64/io.hpp"
#include "kernel/arch/x86_64/pic.hpp"
#include "kernel/drivers/serial.hpp"

namespace {

constexpr uint16_t kPitChannel0 = 0x40;
constexpr uint16_t kPitCommand = 0x43;
constexpr uint32_t kPitBaseFrequency = 1193182;
constexpr uint8_t kPitCommandRateGenerator = 0x36;

volatile uint64_t g_ticks = 0;

} // namespace

namespace kernel::timer {

void init() {
    g_ticks = 0;

    const uint16_t divisor = static_cast<uint16_t>(kPitBaseFrequency / frequency_hz);
    kernel::arch::x86_64::io::outb(kPitCommand, kPitCommandRateGenerator);
    kernel::arch::x86_64::io::outb(kPitChannel0, static_cast<uint8_t>(divisor & 0xff));
    kernel::arch::x86_64::io::outb(kPitChannel0, static_cast<uint8_t>((divisor >> 8) & 0xff));
    kernel::arch::x86_64::pic::unmask(0);

    serial::write_line("os-lab: PIT timer configured at 100 Hz");
}

uint64_t ticks() { return g_ticks; }

void handle_tick() { g_ticks = g_ticks + 1; }

} // namespace kernel::timer
