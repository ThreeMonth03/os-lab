#include "kernel/memory/heap.hpp"

#include "kernel/arch/x86_64/active_paging.hpp"
#include "kernel/memory/memory.hpp"

namespace
{

namespace heap = kernel::memory::heap;
namespace paging = kernel::arch::x86_64::paging;

constexpr size_t kAllocationOverheadEstimate = 128;

[[nodiscard]] bool is_power_of_two(size_t value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

class KernelHeapState
{
public:
    bool init()
    {
        if (initialized_)
        {
            return true;
        }

        const size_t mapped_pages = grow(heap::kInitialCommitPages);
        initialized_ = mapped_pages > 0;
        return initialized_;
    }

    void * allocate(size_t bytes, size_t alignment)
    {
        if (!initialized_ || bytes == 0 || (alignment != 0 && !is_power_of_two(alignment)))
        {
            ++failed_allocations_;
            return nullptr;
        }

        if (void * memory = allocator_.allocate(bytes, alignment); memory != nullptr)
        {
            return memory;
        }

        const size_t pages = required_pages(bytes, alignment);
        if (pages == 0 || grow(pages) == 0)
        {
            ++failed_allocations_;
            return nullptr;
        }

        void * memory = allocator_.allocate(bytes, alignment);
        if (memory == nullptr)
        {
            ++failed_allocations_;
        }
        return memory;
    }

    bool free(void * memory)
    {
        if (!initialized_)
        {
            return false;
        }
        return allocator_.free(memory);
    }

    heap::Stats stats() const
    {
        return {
            initialized_,
            heap::kVirtualBase,
            heap::kMaxBytes,
            committed_bytes_,
            committed_bytes_ / paging::kPageSize,
            failed_allocations_,
            allocator_.stats(),
        };
    }

    kernel::memory::HeapValidationResult validate() const
    {
        kernel::memory::HeapValidationResult result = allocator_.validate();
        if (committed_bytes_ > heap::kMaxBytes ||
            (committed_bytes_ % paging::kPageSize) != 0 ||
            (initialized_ && !result.observed.initialized))
        {
            result.valid = false;
            result.error = kernel::memory::HeapValidationError::RegionStatsMismatch;
        }
        return result;
    }

private:
    [[nodiscard]] size_t required_pages(size_t bytes, size_t alignment) const
    {
        if (alignment > static_cast<size_t>(-1) - bytes ||
            bytes + alignment > static_cast<size_t>(-1) - kAllocationOverheadEstimate)
        {
            return 0;
        }

        const size_t needed_bytes = bytes + alignment + kAllocationOverheadEstimate;
        size_t pages = heap::page_count_for_bytes(needed_bytes);
        if (pages == 0)
        {
            return 0;
        }

        const size_t remaining_pages = (heap::kMaxBytes - committed_bytes_) / paging::kPageSize;
        if (pages > remaining_pages)
        {
            pages = remaining_pages;
        }
        return pages;
    }

    [[nodiscard]] size_t grow(size_t pages)
    {
        if (pages == 0 || committed_bytes_ >= heap::kMaxBytes)
        {
            return 0;
        }

        paging::ActivePageTable active;
        if (!paging::ActivePageTable::current(active))
        {
            return 0;
        }

        const size_t start_offset = committed_bytes_;
        size_t mapped_pages = 0;
        while (mapped_pages < pages && committed_bytes_ < heap::kMaxBytes)
        {
            kernel::memory::PhysicalFrame frame;
            if (!kernel::memory::allocate_frame(frame))
            {
                break;
            }

            const uint64_t virtual_address = heap::kVirtualBase + committed_bytes_;
            const paging::MapResult result =
                active.map_page(virtual_address, frame.address(), paging::PageFlag::Writable | paging::PageFlag::NoExecute);
            if (result != paging::MapResult::Mapped)
            {
                break;
            }

            committed_bytes_ += paging::kPageSize;
            ++mapped_pages;
        }

        if (mapped_pages > 0)
        {
            (void)allocator_.add_region(reinterpret_cast<void *>(heap::kVirtualBase + start_offset),
                                        mapped_pages * paging::kPageSize);
        }
        return mapped_pages;
    }

    kernel::memory::HeapAllocator allocator_;
    size_t committed_bytes_ = 0;
    size_t failed_allocations_ = 0;
    bool initialized_ = false;
};

KernelHeapState g_heap;

} // namespace

namespace kernel::memory::heap
{

bool init() { return g_heap.init(); }

void * allocate(size_t bytes, size_t alignment) { return g_heap.allocate(bytes, alignment); }

bool free(void * memory) { return g_heap.free(memory); }

Stats stats() { return g_heap.stats(); }

HeapValidationResult validate() { return g_heap.validate(); }

} // namespace kernel::memory::heap
