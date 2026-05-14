#pragma once

#include <stddef.h>

#include "kernel/memory/slab_cache.hpp"

namespace kernel::memory::slab
{

inline constexpr size_t kDefaultObjectSize = 64;
inline constexpr size_t kDefaultAlignment = 16;
inline constexpr size_t kBackingPageCount = 1;

struct Stats
{
    bool initialized = false;
    size_t failed_backing_allocations = 0;
    SlabCacheStats cache;
};

[[nodiscard]] bool init();
[[nodiscard]] void * allocate();
[[nodiscard]] bool free(void * memory);
[[nodiscard]] Stats stats();
[[nodiscard]] SlabValidationResult validate();

} // namespace kernel::memory::slab
