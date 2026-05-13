#include <stddef.h>
#include <stdint.h>

#include "kernel/halt.hpp"

extern "C" void* memcpy(void* destination, const void* source, size_t size) {
    auto* dst = static_cast<uint8_t*>(destination);
    const auto* src = static_cast<const uint8_t*>(source);

    for (size_t index = 0; index < size; ++index) {
        dst[index] = src[index];
    }

    return destination;
}

extern "C" void* memmove(void* destination, const void* source, size_t size) {
    auto* dst = static_cast<uint8_t*>(destination);
    const auto* src = static_cast<const uint8_t*>(source);

    if (dst < src) {
        for (size_t index = 0; index < size; ++index) {
            dst[index] = src[index];
        }
    } else if (dst > src) {
        for (size_t index = size; index > 0; --index) {
            dst[index - 1] = src[index - 1];
        }
    }

    return destination;
}

extern "C" void* memset(void* destination, int value, size_t size) {
    auto* dst = static_cast<uint8_t*>(destination);

    for (size_t index = 0; index < size; ++index) {
        dst[index] = static_cast<uint8_t>(value);
    }

    return destination;
}

extern "C" int memcmp(const void* left, const void* right, size_t size) {
    const auto* lhs = static_cast<const uint8_t*>(left);
    const auto* rhs = static_cast<const uint8_t*>(right);

    for (size_t index = 0; index < size; ++index) {
        if (lhs[index] != rhs[index]) {
            return static_cast<int>(lhs[index]) - static_cast<int>(rhs[index]);
        }
    }

    return 0;
}

extern "C" int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }

extern "C" void __cxa_pure_virtual() { kernel::halt_forever(); }

void* __dso_handle = nullptr;
