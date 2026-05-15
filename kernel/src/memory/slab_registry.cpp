#include "kernel/memory/slab_registry.hpp"

namespace kernel::memory
{

void SlabRegistry::reset(Entry * entries, size_t capacity)
{
    entries_ = entries;
    capacity_ = entries == nullptr ? 0 : capacity;
    registered_caches_ = 0;
    failed_registrations_ = 0;

    for (size_t index = 0; index < capacity_; ++index)
    {
        entries_[index] = {};
    }
}

bool SlabRegistry::register_cache(SlabCacheId id,
                                  StringView name,
                                  size_t object_size,
                                  size_t alignment)
{
    if (!can_register(id, name))
    {
        return fail_registration();
    }

    Entry & entry = entries_[id];
    if (!entry.cache.init(object_size, alignment))
    {
        return fail_registration();
    }

    entry.name = name;
    entry.failed_backing_allocations = 0;
    entry.registered = true;
    ++registered_caches_;
    return true;
}

bool SlabRegistry::register_cache(StringView name,
                                  size_t object_size,
                                  size_t alignment,
                                  SlabCacheId & id)
{
    id = kInvalidSlabCacheId;
    if (entries_ == nullptr || name.empty() || find(name) != kInvalidSlabCacheId)
    {
        return fail_registration();
    }

    for (size_t index = 0; index < capacity_; ++index)
    {
        if (!entries_[index].registered)
        {
            if (!register_cache(index, name, object_size, alignment))
            {
                return false;
            }
            id = index;
            return true;
        }
    }

    return fail_registration();
}

SlabCacheId SlabRegistry::find(StringView name) const
{
    if (entries_ == nullptr || name.empty())
    {
        return kInvalidSlabCacheId;
    }

    for (size_t index = 0; index < capacity_; ++index)
    {
        if (entries_[index].registered && entries_[index].name == name)
        {
            return index;
        }
    }
    return kInvalidSlabCacheId;
}

SlabCache * SlabRegistry::lookup(SlabCacheId id)
{
    if (!valid_id(id) || !entries_[id].registered)
    {
        return nullptr;
    }
    return &entries_[id].cache;
}

const SlabCache * SlabRegistry::lookup(SlabCacheId id) const
{
    if (!valid_id(id) || !entries_[id].registered)
    {
        return nullptr;
    }
    return &entries_[id].cache;
}

StringView SlabRegistry::name(SlabCacheId id) const
{
    if (!valid_id(id) || !entries_[id].registered)
    {
        return {};
    }
    return entries_[id].name;
}

bool SlabRegistry::add_slab(SlabCacheId id, void * memory, size_t bytes)
{
    SlabCache * cache = lookup(id);
    if (cache == nullptr)
    {
        return false;
    }
    return cache->add_slab(memory, bytes);
}

void * SlabRegistry::allocate(SlabCacheId id)
{
    SlabCache * cache = lookup(id);
    if (cache == nullptr)
    {
        return nullptr;
    }
    return cache->allocate();
}

bool SlabRegistry::free(SlabCacheId id, void * memory)
{
    SlabCache * cache = lookup(id);
    if (cache == nullptr)
    {
        return false;
    }
    return cache->free(memory);
}

bool SlabRegistry::free(void * memory)
{
    if (memory == nullptr)
    {
        return true;
    }
    if (entries_ == nullptr)
    {
        return false;
    }

    for (size_t index = 0; index < capacity_; ++index)
    {
        if (entries_[index].registered && entries_[index].cache.free(memory))
        {
            return true;
        }
    }
    return false;
}

void SlabRegistry::record_failed_backing_allocation(SlabCacheId id)
{
    if (valid_id(id) && entries_[id].registered)
    {
        ++entries_[id].failed_backing_allocations;
    }
}

SlabRegistryStats SlabRegistry::stats() const
{
    SlabRegistryStats result;
    result.capacity = capacity_;
    result.registered_caches = registered_caches_;
    result.failed_registrations = failed_registrations_;

    if (entries_ == nullptr)
    {
        return result;
    }

    for (size_t index = 0; index < capacity_; ++index)
    {
        if (entries_[index].registered)
        {
            result.failed_backing_allocations += entries_[index].failed_backing_allocations;
        }
    }
    return result;
}

SlabRegistryCacheStats SlabRegistry::cache_stats(SlabCacheId id) const
{
    SlabRegistryCacheStats result;
    result.id = id;
    if (!valid_id(id) || !entries_[id].registered)
    {
        return result;
    }

    result.registered = true;
    result.name = entries_[id].name;
    result.failed_backing_allocations = entries_[id].failed_backing_allocations;
    result.cache = entries_[id].cache.stats();
    return result;
}

SlabRegistryValidationResult SlabRegistry::validate_all() const
{
    SlabRegistryValidationResult result;
    if (entries_ == nullptr && capacity_ != 0)
    {
        result.valid = false;
        result.error = SlabRegistryValidationError::MissingStorage;
        return result;
    }

    size_t observed_registered = 0;
    for (size_t index = 0; index < capacity_; ++index)
    {
        if (!entries_[index].registered)
        {
            continue;
        }

        ++observed_registered;
        for (size_t other = index + 1; other < capacity_; ++other)
        {
            if (entries_[other].registered && entries_[index].name == entries_[other].name)
            {
                result.valid = false;
                result.error = SlabRegistryValidationError::DuplicateName;
                result.cache_id = index;
                return result;
            }
        }

        const SlabValidationResult cache_validation = entries_[index].cache.validate();
        if (!cache_validation.valid)
        {
            result.valid = false;
            result.error = SlabRegistryValidationError::CacheInvalid;
            result.cache_id = index;
            result.cache_error = cache_validation.error;
            return result;
        }
        ++result.validated_caches;
    }

    if (observed_registered != registered_caches_)
    {
        result.valid = false;
        result.error = SlabRegistryValidationError::StatsMismatch;
    }
    return result;
}

bool SlabRegistry::valid_id(SlabCacheId id) const
{
    return entries_ != nullptr && id < capacity_;
}

bool SlabRegistry::can_register(SlabCacheId id, StringView name) const
{
    return valid_id(id) && !name.empty() && !entries_[id].registered &&
           find(name) == kInvalidSlabCacheId;
}

bool SlabRegistry::fail_registration()
{
    ++failed_registrations_;
    return false;
}

} // namespace kernel::memory
