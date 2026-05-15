#include "kernel/arch/x86_64/idt.hpp"

#include <stddef.h>
#include <stdint.h>

#include "kernel/core/halt.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/base/string_view.hpp"
#include "kernel/console/terminal.hpp"

namespace
{

struct [[gnu::packed]] IdtEntry
{
    uint16_t offset_low = 0;
    uint16_t selector = 0;
    uint8_t ist = 0;
    uint8_t attributes = 0;
    uint16_t offset_mid = 0;
    uint32_t offset_high = 0;
    uint32_t reserved = 0;
};

struct [[gnu::packed]] Idtr
{
    uint16_t limit = 0;
    uint64_t base = 0;
};

struct ExceptionFrame
{
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

static_assert(offsetof(ExceptionFrame, vector) == 120);
static_assert(offsetof(ExceptionFrame, rip) == 136);
static_assert(sizeof(ExceptionFrame) == 160);

alignas(16) IdtEntry g_idt[256] = {};

constexpr uint64_t kPageFaultProtectionBit = 1ull << 0;
constexpr uint64_t kPageFaultWriteBit = 1ull << 1;
constexpr uint64_t kPageFaultUserBit = 1ull << 2;
constexpr uint64_t kPageFaultReservedBit = 1ull << 3;
constexpr uint64_t kPageFaultInstructionFetchBit = 1ull << 4;

extern "C" void kernel_x86_64_exception_0();
extern "C" void kernel_x86_64_exception_6();
extern "C" void kernel_x86_64_exception_13();
extern "C" void kernel_x86_64_exception_14();

[[nodiscard]] uint16_t current_code_selector()
{
    uint16_t selector = 0;
    asm volatile("movw %%cs, %0" : "=rm"(selector));
    return selector;
}

void load_idt(const Idtr & idtr) { asm volatile("lidt %0" : : "m"(idtr) : "memory"); }

kernel::StringView exception_name(uint64_t vector)
{
    switch (vector)
    {
    case 0:
        return "divide error";
    case 6:
        return "invalid opcode";
    case 13:
        return "general protection fault";
    case 14:
        return "page fault";
    default:
        return "unknown exception";
    }
}

void write_both(kernel::StringView value)
{
    kernel::drivers::serial::write_string(value);
    if (kernel::console::terminal::ready())
    {
        kernel::console::terminal::write_string(value);
    }
}

void write_both_line(kernel::StringView value)
{
    kernel::drivers::serial::write_line(value);
    if (kernel::console::terminal::ready())
    {
        kernel::console::terminal::write_line(value);
    }
}

void write_both_decimal(uint64_t value)
{
    kernel::drivers::serial::write_decimal(value);
    if (kernel::console::terminal::ready())
    {
        kernel::console::terminal::write_decimal(value);
    }
}

void write_both_hex(uint64_t value)
{
    kernel::drivers::serial::write_hex(value);
    if (kernel::console::terminal::ready())
    {
        kernel::console::terminal::write_hex(value);
    }
}

void write_decimal_field(kernel::StringView name, uint64_t value)
{
    write_both("  ");
    write_both(name);
    write_both(": ");
    write_both_decimal(value);
    write_both_line("");
}

void write_hex_field(kernel::StringView name, uint64_t value)
{
    write_both("  ");
    write_both(name);
    write_both(": ");
    write_both_hex(value);
    write_both_line("");
}

void write_page_fault_error_code(uint64_t error_code)
{
    write_both("  page fault flags: ");
    write_both((error_code & kPageFaultProtectionBit) != 0 ? "protection" : "not-present");
    write_both((error_code & kPageFaultWriteBit) != 0 ? " write" : " read");
    write_both((error_code & kPageFaultUserBit) != 0 ? " user" : " supervisor");
    if ((error_code & kPageFaultReservedBit) != 0)
    {
        write_both(" reserved-bit");
    }
    if ((error_code & kPageFaultInstructionFetchBit) != 0)
    {
        write_both(" instruction-fetch");
    }
    write_both_line("");
}

[[nodiscard]] uint64_t read_cr2()
{
    uint64_t value = 0;
    asm volatile("mov %%cr2, %0" : "=r"(value));
    return value;
}

[[nodiscard]] uint64_t interrupted_rsp(const ExceptionFrame * frame)
{
    return reinterpret_cast<uint64_t>(frame) + sizeof(ExceptionFrame);
}

} // namespace

extern "C" [[noreturn]] void kernel_x86_64_exception_dispatch(const ExceptionFrame * frame)
{
    write_both_line("");
    write_both_line("kernel exception");
    write_both("  name: ");
    write_both_line(exception_name(frame->vector));
    write_decimal_field("vector", frame->vector);
    write_hex_field("error code", frame->error_code);
    write_hex_field("rip", frame->rip);
    write_hex_field("rsp", interrupted_rsp(frame));
    write_hex_field("rflags", frame->rflags);
    write_hex_field("cs", frame->cs);

    if (frame->vector == 14)
    {
        write_hex_field("cr2", read_cr2());
        write_page_fault_error_code(frame->error_code);
    }

    write_both_line("kernel halted");
    kernel::halt_forever();
}

namespace kernel::arch::x86_64
{

void set_interrupt_gate(uint8_t vector, void (*handler)())
{
    const auto address = reinterpret_cast<uint64_t>(handler);

    g_idt[vector].offset_low = static_cast<uint16_t>(address & 0xffff);
    g_idt[vector].selector = current_code_selector();
    g_idt[vector].ist = 0;
    g_idt[vector].attributes = 0x8e;
    g_idt[vector].offset_mid = static_cast<uint16_t>((address >> 16) & 0xffff);
    g_idt[vector].offset_high = static_cast<uint32_t>((address >> 32) & 0xffffffff);
    g_idt[vector].reserved = 0;
}

void init_exceptions()
{
    set_interrupt_gate(0, kernel_x86_64_exception_0);
    set_interrupt_gate(6, kernel_x86_64_exception_6);
    set_interrupt_gate(13, kernel_x86_64_exception_13);
    set_interrupt_gate(14, kernel_x86_64_exception_14);

    const Idtr idtr = {static_cast<uint16_t>(sizeof(g_idt) - 1),
                       reinterpret_cast<uint64_t>(&g_idt)};
    load_idt(idtr);
    kernel::drivers::serial::write_line("os-lab: x86_64 exception handlers loaded");
}

} // namespace kernel::arch::x86_64
