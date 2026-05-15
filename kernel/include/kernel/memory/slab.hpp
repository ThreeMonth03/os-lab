#pragma once

#include <stddef.h>

#include "kernel/memory/slab_cache.hpp"
#include "kernel/memory/slab_registry.hpp"

namespace kernel::memory::slab
{

inline constexpr SlabCacheId kDefaultCacheId = 0;
inline constexpr size_t kMaxKernelCaches = 8;
inline constexpr size_t kDefaultObjectSize = 64;
inline constexpr size_t kDefaultAlignment = 16;
inline constexpr size_t kBackingPageCount = 1;

struct Stats
{
    bool initialized = false;
    size_t failed_backing_allocations = 0;
    SlabCacheStats cache;
    SlabRegistryStats registry;
};

[[nodiscard]] bool init();
[[nodiscard]] SlabCacheId create_cache(StringView name, size_t object_size, size_t alignment);
[[nodiscard]] SlabCacheId find_cache(StringView name);
[[nodiscard]] void * allocate();
[[nodiscard]] void * allocate(SlabCacheId id);
[[nodiscard]] bool free(void * memory);
[[nodiscard]] bool free(SlabCacheId id, void * memory);
[[nodiscard]] Stats stats();
[[nodiscard]] SlabRegistryCacheStats cache_stats(SlabCacheId id);
[[nodiscard]] SlabValidationResult validate();
[[nodiscard]] SlabRegistryValidationResult validate_all();

} // namespace kernel::memory::slab
