#include "kernel/memory/slab.hpp"

#include "kernel/arch/x86_64/paging.hpp"
#include "kernel/memory/heap.hpp"

namespace
{

namespace paging = kernel::arch::x86_64::paging;
namespace slab = kernel::memory::slab;

class KernelSlabState
{
public:
    bool init()
    {
        if (!init_registry())
        {
            return false;
        }

        if (initialized_)
        {
            return true;
        }

        initialized_ = grow(slab::kDefaultCacheId);
        return initialized_;
    }

    kernel::memory::SlabCacheId create_cache(kernel::StringView name,
                                             size_t object_size,
                                             size_t alignment)
    {
        if (!init_registry())
        {
            return kernel::memory::kInvalidSlabCacheId;
        }

        kernel::memory::SlabCacheId id = kernel::memory::kInvalidSlabCacheId;
        if (!registry_.register_cache(name, object_size, alignment, id))
        {
            return kernel::memory::kInvalidSlabCacheId;
        }
        return id;
    }

    kernel::memory::SlabCacheId find_cache(kernel::StringView name) const
    {
        return registry_.find(name);
    }

    void * allocate(kernel::memory::SlabCacheId id)
    {
        if (!registry_ready_)
        {
            return nullptr;
        }

        const kernel::memory::SlabRegistryCacheStats stats = registry_.cache_stats(id);
        if (!stats.registered)
        {
            return nullptr;
        }
        if (stats.cache.free_objects == 0 && !grow(id))
        {
            return nullptr;
        }
        return registry_.allocate(id);
    }

    bool free(kernel::memory::SlabCacheId id, void * memory)
    {
        if (!registry_ready_)
        {
            return false;
        }
        return registry_.free(id, memory);
    }

    bool free(void * memory)
    {
        if (!registry_ready_)
        {
            return false;
        }
        return registry_.free(memory);
    }

    slab::Stats stats() const
    {
        const kernel::memory::SlabRegistryCacheStats default_cache =
            registry_.cache_stats(slab::kDefaultCacheId);
        const kernel::memory::SlabRegistryStats registry_stats = registry_.stats();
        return {
            initialized_,
            registry_stats.failed_backing_allocations,
            default_cache.cache,
            registry_stats,
        };
    }

    kernel::memory::SlabRegistryCacheStats cache_stats(kernel::memory::SlabCacheId id) const
    {
        return registry_.cache_stats(id);
    }

    kernel::memory::SlabValidationResult validate() const
    {
        const kernel::memory::SlabCache * cache = registry_.lookup(slab::kDefaultCacheId);
        if (cache == nullptr)
        {
            return {
                false,
                kernel::memory::SlabValidationError::StatsMismatch,
                {},
            };
        }

        kernel::memory::SlabValidationResult result = cache->validate();
        if (initialized_ && !result.observed.initialized)
        {
            result.valid = false;
            result.error = kernel::memory::SlabValidationError::StatsMismatch;
        }
        return result;
    }

    kernel::memory::SlabRegistryValidationResult validate_all() const
    {
        return registry_.validate_all();
    }

private:
    bool init_registry()
    {
        if (registry_ready_)
        {
            return true;
        }

        registry_.reset(entries_, slab::kMaxKernelCaches);
        registry_ready_ = registry_.register_cache(slab::kDefaultCacheId,
                                                   "default64",
                                                   slab::kDefaultObjectSize,
                                                   slab::kDefaultAlignment);
        return registry_ready_;
    }

    bool grow(kernel::memory::SlabCacheId id)
    {
        void * memory = kernel::memory::heap::allocate(slab::kBackingPageCount * paging::kPageSize,
                                                       paging::kPageSize);
        if (memory == nullptr)
        {
            registry_.record_failed_backing_allocation(id);
            return false;
        }
        if (!registry_.add_slab(id, memory, slab::kBackingPageCount * paging::kPageSize))
        {
            (void)kernel::memory::heap::free(memory);
            registry_.record_failed_backing_allocation(id);
            return false;
        }
        return true;
    }

    kernel::memory::SlabRegistry::Entry entries_[slab::kMaxKernelCaches] = {};
    kernel::memory::SlabRegistry registry_;
    bool registry_ready_ = false;
    bool initialized_ = false;
};

KernelSlabState g_slab;

} // namespace

namespace kernel::memory::slab
{

bool init() { return g_slab.init(); }

SlabCacheId create_cache(StringView name, size_t object_size, size_t alignment)
{
    return g_slab.create_cache(name, object_size, alignment);
}

SlabCacheId find_cache(StringView name) { return g_slab.find_cache(name); }

void * allocate() { return g_slab.allocate(kDefaultCacheId); }

void * allocate(SlabCacheId id) { return g_slab.allocate(id); }

bool free(void * memory) { return g_slab.free(memory); }

bool free(SlabCacheId id, void * memory) { return g_slab.free(id, memory); }

Stats stats() { return g_slab.stats(); }

SlabRegistryCacheStats cache_stats(SlabCacheId id) { return g_slab.cache_stats(id); }

SlabValidationResult validate() { return g_slab.validate(); }

SlabRegistryValidationResult validate_all() { return g_slab.validate_all(); }

} // namespace kernel::memory::slab
