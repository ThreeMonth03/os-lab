#pragma once

#include <stddef.h>

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <new>
#else
[[nodiscard]] inline void* operator new(size_t, void* pointer) noexcept { return pointer; }

inline void operator delete(void*, void*) noexcept {}
#endif
