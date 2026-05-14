#include "kernel/runtime/runtime.hpp"

namespace
{

using Constructor = void (*)();

extern "C" Constructor __init_array_start[];
extern "C" Constructor __init_array_end[];

} // namespace

namespace kernel::runtime
{

void run_constructors()
{
    for (Constructor * constructor = __init_array_start; constructor != __init_array_end;
         ++constructor)
    {
        (*constructor)();
    }
}

} // namespace kernel::runtime
