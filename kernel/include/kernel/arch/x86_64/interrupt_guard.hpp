#pragma once

#include <stdint.h>

namespace kernel::arch::x86_64
{

[[nodiscard]] inline uint64_t read_rflags()
{
    uint64_t flags = 0;
    asm volatile("pushfq\n\t"
                 "popq %0"
                 : "=r"(flags)
                 :
                 : "memory");
    return flags;
}

[[nodiscard]] inline bool interrupts_enabled()
{
    constexpr uint64_t interrupt_flag = 1ull << 9;
    return (read_rflags() & interrupt_flag) != 0;
}

inline void enable_interrupts_local() { asm volatile("sti" ::: "memory"); }

inline void disable_interrupts_local() { asm volatile("cli" ::: "memory"); }

class InterruptGuard
{
public:
    InterruptGuard()
        : restore_enabled_(interrupts_enabled())
    {
        disable_interrupts_local();
    }

    InterruptGuard(const InterruptGuard &) = delete;
    InterruptGuard & operator=(const InterruptGuard &) = delete;
    InterruptGuard(InterruptGuard &&) = delete;
    InterruptGuard & operator=(InterruptGuard &&) = delete;

    ~InterruptGuard()
    {
        if (restore_enabled_)
        {
            enable_interrupts_local();
        }
    }

private:
    bool restore_enabled_ = false;
};

} // namespace kernel::arch::x86_64
