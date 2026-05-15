#pragma once

#include <stddef.h>

#include "kernel/base/string_view.hpp"
#include "kernel/memory/slab_cache.hpp"

namespace kernel::memory
{

using SlabCacheId = size_t;
inline constexpr SlabCacheId kInvalidSlabCacheId = static_cast<SlabCacheId>(-1);

struct SlabRegistryStats
{
    size_t capacity = 0;
    size_t registered_caches = 0;
    size_t failed_registrations = 0;
    size_t failed_backing_allocations = 0;
};

struct SlabRegistryCacheStats
{
    bool registered = false;
    SlabCacheId id = kInvalidSlabCacheId;
    StringView name;
    size_t failed_backing_allocations = 0;
    SlabCacheStats cache;
};

enum class SlabRegistryValidationError
{
    None,
    MissingStorage,
    DuplicateName,
    CacheInvalid,
    StatsMismatch,
};

struct SlabRegistryValidationResult
{
    bool valid = true;
    SlabRegistryValidationError error = SlabRegistryValidationError::None;
    SlabCacheId cache_id = kInvalidSlabCacheId;
    SlabValidationError cache_error = SlabValidationError::None;
    size_t validated_caches = 0;
};

class SlabRegistry
{
public:
    struct Entry
    {
        StringView name;
        SlabCache cache;
        size_t failed_backing_allocations = 0;
        bool registered = false;
    };

    SlabRegistry() = default;

    void reset(Entry * entries, size_t capacity);

    [[nodiscard]] bool register_cache(SlabCacheId id,
                                      StringView name,
                                      size_t object_size,
                                      size_t alignment);
    [[nodiscard]] bool register_cache(StringView name,
                                      size_t object_size,
                                      size_t alignment,
                                      SlabCacheId & id);

    [[nodiscard]] SlabCacheId find(StringView name) const;
    [[nodiscard]] SlabCache * lookup(SlabCacheId id);
    [[nodiscard]] const SlabCache * lookup(SlabCacheId id) const;
    [[nodiscard]] StringView name(SlabCacheId id) const;

    [[nodiscard]] bool add_slab(SlabCacheId id, void * memory, size_t bytes);
    [[nodiscard]] void * allocate(SlabCacheId id);
    [[nodiscard]] bool free(SlabCacheId id, void * memory);
    [[nodiscard]] bool free(void * memory);

    void record_failed_backing_allocation(SlabCacheId id);

    [[nodiscard]] SlabRegistryStats stats() const;
    [[nodiscard]] SlabRegistryCacheStats cache_stats(SlabCacheId id) const;
    [[nodiscard]] SlabRegistryValidationResult validate_all() const;

private:
    [[nodiscard]] bool valid_id(SlabCacheId id) const;
    [[nodiscard]] bool can_register(SlabCacheId id, StringView name) const;
    [[nodiscard]] bool fail_registration();

    Entry * entries_ = nullptr;
    size_t capacity_ = 0;
    size_t registered_caches_ = 0;
    size_t failed_registrations_ = 0;
};

} // namespace kernel::memory
