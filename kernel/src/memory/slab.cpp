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
        if (initialized_)
        {
            return true;
        }
        if (!cache_.init(slab::kDefaultObjectSize, slab::kDefaultAlignment))
        {
            return false;
        }

        initialized_ = grow();
        return initialized_;
    }

    void * allocate()
    {
        if (!initialized_)
        {
            return nullptr;
        }
        if (cache_.stats().free_objects == 0 && !grow())
        {
            return nullptr;
        }
        return cache_.allocate();
    }

    bool free(void * memory)
    {
        if (!initialized_)
        {
            return false;
        }
        return cache_.free(memory);
    }

    slab::Stats stats() const
    {
        return {
            initialized_,
            failed_backing_allocations_,
            cache_.stats(),
        };
    }

    kernel::memory::SlabValidationResult validate() const
    {
        kernel::memory::SlabValidationResult result = cache_.validate();
        if (initialized_ && !result.observed.initialized)
        {
            result.valid = false;
            result.error = kernel::memory::SlabValidationError::StatsMismatch;
        }
        return result;
    }

private:
    bool grow()
    {
        void * memory = kernel::memory::heap::allocate(slab::kBackingPageCount * paging::kPageSize,
                                                       paging::kPageSize);
        if (memory == nullptr)
        {
            ++failed_backing_allocations_;
            return false;
        }
        if (!cache_.add_slab(memory, slab::kBackingPageCount * paging::kPageSize))
        {
            (void)kernel::memory::heap::free(memory);
            ++failed_backing_allocations_;
            return false;
        }
        return true;
    }

    kernel::memory::SlabCache cache_;
    size_t failed_backing_allocations_ = 0;
    bool initialized_ = false;
};

KernelSlabState g_slab;

} // namespace

namespace kernel::memory::slab
{

bool init() { return g_slab.init(); }

void * allocate() { return g_slab.allocate(); }

bool free(void * memory) { return g_slab.free(memory); }

Stats stats() { return g_slab.stats(); }

SlabValidationResult validate() { return g_slab.validate(); }

} // namespace kernel::memory::slab
