#include "kernel/arch/x86_64/irq.hpp"

#include <stdint.h>

#include "kernel/arch/x86_64/idt.hpp"
#include "kernel/arch/x86_64/pic.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/time/timer.hpp"

namespace {

constexpr uint32_t kIa32ApicBase = 0x1b;
constexpr uint64_t kApicGlobalEnable = 1ull << 11;

struct IrqFrame {
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

extern "C" void kernel_x86_64_irq_32();
extern "C" void kernel_x86_64_irq_33();
extern "C" void kernel_x86_64_irq_34();
extern "C" void kernel_x86_64_irq_35();
extern "C" void kernel_x86_64_irq_36();
extern "C" void kernel_x86_64_irq_37();
extern "C" void kernel_x86_64_irq_38();
extern "C" void kernel_x86_64_irq_39();
extern "C" void kernel_x86_64_irq_40();
extern "C" void kernel_x86_64_irq_41();
extern "C" void kernel_x86_64_irq_42();
extern "C" void kernel_x86_64_irq_43();
extern "C" void kernel_x86_64_irq_44();
extern "C" void kernel_x86_64_irq_45();
extern "C" void kernel_x86_64_irq_46();
extern "C" void kernel_x86_64_irq_47();

using Handler = void (*)();

constexpr Handler kIrqHandlers[] = {
    kernel_x86_64_irq_32, kernel_x86_64_irq_33, kernel_x86_64_irq_34, kernel_x86_64_irq_35,
    kernel_x86_64_irq_36, kernel_x86_64_irq_37, kernel_x86_64_irq_38, kernel_x86_64_irq_39,
    kernel_x86_64_irq_40, kernel_x86_64_irq_41, kernel_x86_64_irq_42, kernel_x86_64_irq_43,
    kernel_x86_64_irq_44, kernel_x86_64_irq_45, kernel_x86_64_irq_46, kernel_x86_64_irq_47,
};

static_assert(sizeof(kIrqHandlers) / sizeof(kIrqHandlers[0]) ==
              kernel::arch::x86_64::pic::irq_count);

[[nodiscard]] uint64_t read_msr(uint32_t msr) {
    uint32_t low = 0;
    uint32_t high = 0;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return (static_cast<uint64_t>(high) << 32) | low;
}

void write_msr(uint32_t msr, uint64_t value) {
    asm volatile("wrmsr"
                 :
                 : "c"(msr), "a"(static_cast<uint32_t>(value & 0xffffffff)),
                   "d"(static_cast<uint32_t>(value >> 32)));
}

void disable_local_apic_for_pic() {
    const uint64_t apic_base = read_msr(kIa32ApicBase);
    write_msr(kIa32ApicBase, apic_base & ~kApicGlobalEnable);
}

} // namespace

extern "C" void kernel_x86_64_irq_dispatch(const IrqFrame* frame) {
    const uint8_t irq_line =
        static_cast<uint8_t>(frame->vector - kernel::arch::x86_64::pic::vector_base);

    if (irq_line == 0) {
        kernel::timer::handle_tick();
    }

    kernel::arch::x86_64::pic::send_eoi(irq_line);
}

namespace kernel::arch::x86_64 {

void init_irqs() {
    for (uint8_t index = 0; index < pic::irq_count; ++index) {
        set_interrupt_gate(static_cast<uint8_t>(pic::vector_base + index), kIrqHandlers[index]);
    }

    disable_local_apic_for_pic();
    pic::remap();
    pic::mask_all();
    serial::write_line("os-lab: x86_64 IRQ foundation loaded");
}

void enable_interrupts() { asm volatile("sti" ::: "memory"); }

void disable_interrupts() { asm volatile("cli" ::: "memory"); }

} // namespace kernel::arch::x86_64

#define IRQ_STUB(vector)                                                                           \
    ".global kernel_x86_64_irq_" #vector "\n"                                                      \
    "kernel_x86_64_irq_" #vector ":\n"                                                             \
    "    pushq $0\n"                                                                               \
    "    pushq $" #vector "\n"                                                                     \
    "    jmp kernel_x86_64_irq_common\n"

asm(R"(
.pushsection .text
)" IRQ_STUB(32) IRQ_STUB(33) IRQ_STUB(34) IRQ_STUB(35) IRQ_STUB(36) IRQ_STUB(37) IRQ_STUB(38)
        IRQ_STUB(39) IRQ_STUB(40) IRQ_STUB(41) IRQ_STUB(42) IRQ_STUB(43) IRQ_STUB(44) IRQ_STUB(45)
            IRQ_STUB(46) IRQ_STUB(47) R"(
kernel_x86_64_irq_common:
    cld
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rdi
    pushq %rsi
    pushq %rbp
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax
    movq %rsp, %rdi
    call kernel_x86_64_irq_dispatch
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rbp
    popq %rsi
    popq %rdi
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    addq $16, %rsp
    iretq

.popsection
)");
