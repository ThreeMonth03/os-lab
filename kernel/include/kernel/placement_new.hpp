#pragma once

#include <stddef.h>

[[nodiscard]] inline void* operator new(size_t, void* pointer) noexcept { return pointer; }

inline void operator delete(void*, void*) noexcept {}
