#pragma once

namespace kernel
{

[[noreturn]] inline void halt_forever()
{
    for (;;)
    {
        asm volatile("cli; hlt" ::: "memory");
    }
}

} // namespace kernel
