#pragma once

#include <stddef.h>

#include "kernel/memory/slab_cache.hpp"

namespace kernel::memory
{

template <typename T>
class TypedSlabCache
{
public:
    static constexpr size_t kObjectSize = sizeof(T);
    static constexpr size_t kAlignment = alignof(T);

    TypedSlabCache() = default;

    [[nodiscard]] bool init() { return cache_.init(kObjectSize, kAlignment); }
    [[nodiscard]] bool add_slab(void * memory, size_t bytes) { return cache_.add_slab(memory, bytes); }

    [[nodiscard]] void * allocate_raw() { return cache_.allocate(); }
    [[nodiscard]] T * allocate_uninitialized()
    {
        return static_cast<T *>(allocate_raw());
    }

    [[nodiscard]] bool free_raw(void * memory) { return cache_.free(memory); }
    [[nodiscard]] bool free_uninitialized(T * memory) { return cache_.free(memory); }

    SlabCacheStats stats() const { return cache_.stats(); }
    [[nodiscard]] SlabValidationResult validate() const { return cache_.validate(); }
    bool initialized() const { return cache_.initialized(); }

private:
    SlabCache cache_;
};

} // namespace kernel::memory
