#include "kernel/arch/x86_64/idt.hpp"
#include "kernel/arch/x86_64/irq.hpp"
#include "kernel/drivers/debug_port.hpp"
#include "kernel/halt.hpp"
#include "kernel/limine_support.hpp"
#include "kernel/runtime.hpp"
#include "kernel/drivers/serial.hpp"

extern "C" [[noreturn]] void kernel_main();

extern "C" [[gnu::section(".text._start"), noreturn]] void _start() {
    kernel::debug_port::write_string("os-lab: reached _start\n");
    kernel::serial::init();
    kernel::arch::x86_64::init_exceptions();
    kernel::arch::x86_64::init_irqs();

    if (!kernel::boot::base_revision_supported()) {
        kernel::debug_port::write_string("os-lab: unsupported base revision\n");
        kernel::serial::write_line("os-lab: unsupported Limine base revision");
        kernel::halt_forever();
    }

    kernel::debug_port::write_string("os-lab: entering kernel_main\n");
    kernel::runtime::run_constructors();
    kernel_main();
}
