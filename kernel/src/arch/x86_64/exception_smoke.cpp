#include "kernel/arch/x86_64/exception_smoke.hpp"

#include "kernel/core/halt.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/console/terminal.hpp"

#ifndef OS_LAB_EXCEPTION_SMOKE
#define OS_LAB_EXCEPTION_SMOKE 0
#endif

namespace
{

[[maybe_unused]] void write_trigger(kernel::StringView name)
{
    kernel::drivers::serial::write_string("os-lab: triggering exception smoke: ");
    kernel::drivers::serial::write_line(name);

    if (kernel::console::terminal::ready())
    {
        kernel::console::terminal::write_string("triggering exception smoke: ");
        kernel::console::terminal::write_line(name);
    }
}

[[maybe_unused, noreturn]] void trigger_invalid_opcode()
{
    write_trigger("invalid opcode");
    asm volatile("ud2");
    kernel::halt_forever();
}

[[maybe_unused, noreturn]] void trigger_page_fault()
{
    write_trigger("page fault");
    asm volatile("xorq %%rax, %%rax; movq (%%rax), %%rax" ::: "rax", "memory");
    kernel::halt_forever();
}

[[maybe_unused, noreturn]] void trigger_divide_error()
{
    write_trigger("divide error");
    asm volatile("xorq %%rdx, %%rdx; movq $1, %%rax; xorq %%rcx, %%rcx; divq %%rcx"
                 :
                 :
                 : "rax", "rcx", "rdx", "memory");
    kernel::halt_forever();
}

} // namespace

namespace kernel::arch::x86_64
{

void run_exception_smoke()
{
#if OS_LAB_EXCEPTION_SMOKE == 1
    trigger_invalid_opcode();
#elif OS_LAB_EXCEPTION_SMOKE == 2
    trigger_page_fault();
#elif OS_LAB_EXCEPTION_SMOKE == 3
    trigger_divide_error();
#elif OS_LAB_EXCEPTION_SMOKE == 0
    return;
#else
#error "Unsupported OS_LAB_EXCEPTION_SMOKE value"
#endif
}

} // namespace kernel::arch::x86_64
