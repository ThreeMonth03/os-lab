#pragma once

#include "kernel/base/string_view.hpp"

namespace kernel {

[[noreturn]] void panic(StringView message);
[[noreturn]] void panic_assert(StringView condition, StringView file, int line);

} // namespace kernel

#define KERNEL_ASSERT(condition)                                                                   \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            ::kernel::panic_assert(#condition, __FILE__, __LINE__);                                \
        }                                                                                          \
    } while (false)
