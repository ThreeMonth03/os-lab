#include "kernel/memory/heap_allocator.hpp"

namespace
{

constexpr size_t kMinimumAlignment = alignof(uintptr_t);
constexpr uint64_t kAllocationMagic = 0x6f736c6162484541ull;

struct AllocationHeader
{
    uint64_t magic = 0;
    uintptr_t block_start = 0;
    size_t block_size = 0;
    size_t requested_size = 0;
};

static_assert(sizeof(AllocationHeader) % kMinimumAlignment == 0);

[[nodiscard]] AllocationHeader * allocation_header_at(uintptr_t address)
{
    // Heap metadata is stored inside managed memory; integer addresses are expected here.
    return reinterpret_cast<AllocationHeader *>(address); // NOLINT(performance-no-int-to-ptr)
}

[[nodiscard]] void * payload_at(uintptr_t address)
{
    // Heap payloads are returned from managed virtual memory addresses.
    return reinterpret_cast<void *>(address); // NOLINT(performance-no-int-to-ptr)
}

} // namespace

namespace kernel::memory
{

struct HeapAllocator::FreeBlock
{
    size_t size = 0;
    FreeBlock * previous = nullptr;
    FreeBlock * next = nullptr;
};

void HeapAllocator::reset(void * memory, size_t bytes)
{
    for (Region & region : regions_)
    {
        region = {};
    }
    region_count_ = 0;
    free_head_ = nullptr;
    region_bytes_ = 0;
    allocated_bytes_ = 0;
    allocation_count_ = 0;
    initialized_ = false;

    uintptr_t start = 0;
    size_t usable_bytes = 0;
    if (!usable_region(memory, bytes, start, usable_bytes))
    {
        return;
    }

    if (!record_region(start, usable_bytes))
    {
        return;
    }

    initialized_ = true;
    insert_free_block(start, usable_bytes);
    region_bytes_ = usable_bytes;
}

bool HeapAllocator::add_region(void * memory, size_t bytes)
{
    uintptr_t start = 0;
    size_t usable_bytes = 0;
    if (!usable_region(memory, bytes, start, usable_bytes))
    {
        return false;
    }
    if (!record_region(start, usable_bytes))
    {
        return false;
    }

    initialized_ = true;
    insert_free_block(start, usable_bytes);
    region_bytes_ += usable_bytes;
    return true;
}

void * HeapAllocator::allocate(size_t bytes, size_t alignment)
{
    if (!initialized_ || bytes == 0)
    {
        return nullptr;
    }

    if (alignment == 0)
    {
        alignment = kMinimumAlignment;
    }
    else if (!is_power_of_two(alignment))
    {
        return nullptr;
    }
    alignment = normalize_alignment(alignment);

    for (FreeBlock * block = free_head_; block != nullptr; block = block->next)
    {
        const auto block_start = reinterpret_cast<uintptr_t>(block);
        const uintptr_t payload =
            align_up(block_start + sizeof(AllocationHeader), alignment);
        const uintptr_t header_address = payload - sizeof(AllocationHeader);
        const uintptr_t allocation_end = align_up(payload + bytes, kMinimumAlignment);
        const size_t used_bytes = allocation_end - block_start;
        if (used_bytes > block->size)
        {
            continue;
        }

        const size_t trailing_bytes = block->size - used_bytes;
        const size_t allocation_size =
            trailing_bytes >= sizeof(FreeBlock) ? used_bytes : block->size;
        if (trailing_bytes >= sizeof(FreeBlock))
        {
            replace_free_block(*block, block_start + used_bytes, trailing_bytes);
        }
        else
        {
            remove_free_block(*block);
        }

        auto * header = allocation_header_at(header_address);
        header->magic = kAllocationMagic;
        header->block_start = block_start;
        header->block_size = allocation_size;
        header->requested_size = bytes;

        allocated_bytes_ += bytes;
        ++allocation_count_;
        return payload_at(payload);
    }

    return nullptr;
}

bool HeapAllocator::free(void * memory)
{
    if (memory == nullptr)
    {
        return true;
    }
    if (!initialized_)
    {
        return false;
    }

    const auto payload = reinterpret_cast<uintptr_t>(memory);
    if (!allocation_header_valid(payload))
    {
        return false;
    }

    auto * header = allocation_header_at(payload - sizeof(AllocationHeader));

    header->magic = 0;
    allocated_bytes_ -= header->requested_size;
    --allocation_count_;
    insert_free_block(header->block_start, header->block_size);
    return true;
}

HeapAllocatorStats HeapAllocator::stats() const
{
    HeapAllocatorStats result;
    result.initialized = initialized_;
    result.region_count = region_count_;
    result.region_bytes = region_bytes_;
    result.allocated_bytes = allocated_bytes_;
    result.allocation_count = allocation_count_;

    for (const FreeBlock * block = free_head_; block != nullptr; block = block->next)
    {
        result.free_bytes += block->size;
        ++result.free_block_count;
        if (block->size > result.largest_free_block)
        {
            result.largest_free_block = block->size;
        }
    }

    return result;
}

HeapValidationResult HeapAllocator::validate() const
{
    HeapValidationResult result;
    result.observed.initialized = initialized_;
    result.observed.region_count = region_count_;
    result.observed.region_bytes = region_bytes_;
    result.observed.allocated_bytes = allocated_bytes_;
    result.observed.allocation_count = allocation_count_;

    size_t region_bytes = 0;
    result.error = validate_regions(region_bytes);
    if (result.error != HeapValidationError::None)
    {
        result.valid = false;
        return result;
    }

    result.error = validate_free_list(result.observed);
    if (result.error != HeapValidationError::None)
    {
        result.valid = false;
        return result;
    }

    if (region_bytes != region_bytes_)
    {
        result.valid = false;
        result.error = HeapValidationError::RegionStatsMismatch;
        return result;
    }

    return result;
}

size_t HeapAllocator::normalize_alignment(size_t alignment)
{
    if (alignment < kMinimumAlignment)
    {
        return kMinimumAlignment;
    }
    return alignment;
}

bool HeapAllocator::is_power_of_two(size_t value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

uintptr_t HeapAllocator::align_up(uintptr_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
}

bool HeapAllocator::usable_region(void * memory,
                                  size_t bytes,
                                  uintptr_t & start,
                                  size_t & usable_bytes)
{
    start = 0;
    usable_bytes = 0;
    if (memory == nullptr)
    {
        return false;
    }

    const auto raw_start = reinterpret_cast<uintptr_t>(memory);
    const uintptr_t aligned_start = align_up(raw_start, kMinimumAlignment);
    const size_t prefix = aligned_start - raw_start;
    if (bytes <= prefix)
    {
        return false;
    }

    const size_t available = bytes - prefix;
    const size_t aligned_bytes = available & ~(kMinimumAlignment - 1);
    if (aligned_bytes < sizeof(FreeBlock))
    {
        return false;
    }

    start = aligned_start;
    usable_bytes = aligned_bytes;
    return true;
}

HeapAllocator::FreeBlock * HeapAllocator::free_block_at(uintptr_t address)
{
    // Free list nodes live inside the free heap blocks they describe.
    return reinterpret_cast<FreeBlock *>(address); // NOLINT(performance-no-int-to-ptr)
}

bool HeapAllocator::record_region(uintptr_t start, size_t bytes)
{
    if ((start % kMinimumAlignment) != 0 || bytes < sizeof(FreeBlock) ||
        (bytes % kMinimumAlignment) != 0 || bytes > UINTPTR_MAX - start)
    {
        return false;
    }

    uintptr_t merged_start = start;
    size_t merged_size = bytes;
    bool merged = true;
    while (merged)
    {
        merged = false;
        const uintptr_t merged_end = merged_start + merged_size;

        for (size_t index = 0; index < region_count_; ++index)
        {
            const uintptr_t region_start = regions_[index].start;
            const uintptr_t region_end = region_start + regions_[index].size;
            const bool overlaps = merged_start < region_end && merged_end > region_start;
            if (overlaps)
            {
                return false;
            }

            if (region_end == merged_start || merged_end == region_start)
            {
                if (merged_size > static_cast<size_t>(-1) - regions_[index].size)
                {
                    return false;
                }
                merged_start = region_end == merged_start ? region_start : merged_start;
                merged_size += regions_[index].size;
                for (size_t move = index + 1; move < region_count_; ++move)
                {
                    regions_[move - 1] = regions_[move];
                }
                regions_[region_count_ - 1] = {};
                --region_count_;
                merged = true;
                break;
            }
        }
    }

    if (region_count_ == kMaxRegions)
    {
        return false;
    }

    size_t insert_at = 0;
    while (insert_at < region_count_ && regions_[insert_at].start < merged_start)
    {
        ++insert_at;
    }

    for (size_t move = region_count_; move > insert_at; --move)
    {
        regions_[move] = regions_[move - 1];
    }
    regions_[insert_at] = {merged_start, merged_size};
    ++region_count_;
    return true;
}

bool HeapAllocator::managed_range(uintptr_t address, size_t bytes) const
{
    if (bytes == 0 || bytes > UINTPTR_MAX - address)
    {
        return false;
    }

    const uintptr_t end = address + bytes;
    for (size_t index = 0; index < region_count_; ++index)
    {
        const uintptr_t region_start = regions_[index].start;
        const uintptr_t region_end = region_start + regions_[index].size;
        if (address >= region_start && end <= region_end)
        {
            return true;
        }
    }
    return false;
}

bool HeapAllocator::overlaps_free_block(uintptr_t start, size_t bytes) const
{
    if (bytes == 0 || bytes > UINTPTR_MAX - start)
    {
        return false;
    }

    const uintptr_t end = start + bytes;
    for (const FreeBlock * block = free_head_; block != nullptr; block = block->next)
    {
        const auto block_start = reinterpret_cast<uintptr_t>(block);
        const uintptr_t block_end = block_start + block->size;
        if (start < block_end && end > block_start)
        {
            return true;
        }
    }
    return false;
}

bool HeapAllocator::allocation_header_valid(uintptr_t payload) const
{
    if (payload < sizeof(AllocationHeader))
    {
        return false;
    }

    const uintptr_t header_address = payload - sizeof(AllocationHeader);
    if (!managed_range(header_address, sizeof(AllocationHeader)))
    {
        return false;
    }

    const AllocationHeader * header = allocation_header_at(header_address);
    if (header->magic != kAllocationMagic || header->block_size < sizeof(FreeBlock) ||
        header->requested_size == 0 || (header->block_start % kMinimumAlignment) != 0 ||
        (header->block_size % kMinimumAlignment) != 0)
    {
        return false;
    }
    if (header->block_size > UINTPTR_MAX - header->block_start)
    {
        return false;
    }

    const uintptr_t block_end = header->block_start + header->block_size;
    if (!managed_range(header->block_start, header->block_size) ||
        header_address < header->block_start ||
        header_address > block_end - sizeof(AllocationHeader) ||
        payload > block_end ||
        header->requested_size > block_end - payload)
    {
        return false;
    }

    return !overlaps_free_block(header->block_start, header->block_size);
}

HeapValidationError HeapAllocator::validate_regions(size_t & bytes) const
{
    bytes = 0;
    uintptr_t previous_end = 0;
    for (size_t index = 0; index < region_count_; ++index)
    {
        const Region & region = regions_[index];
        if ((region.start % kMinimumAlignment) != 0)
        {
            return HeapValidationError::RegionMisaligned;
        }
        if (region.size < sizeof(FreeBlock) || (region.size % kMinimumAlignment) != 0)
        {
            return HeapValidationError::RegionTooSmall;
        }
        if (region.size > UINTPTR_MAX - region.start)
        {
            return HeapValidationError::RegionOverlap;
        }

        const uintptr_t region_end = region.start + region.size;
        if (index > 0 && previous_end > region.start)
        {
            return HeapValidationError::RegionOverlap;
        }
        if (bytes > static_cast<size_t>(-1) - region.size)
        {
            return HeapValidationError::RegionStatsMismatch;
        }

        bytes += region.size;
        previous_end = region_end;
    }

    if (bytes != region_bytes_)
    {
        return HeapValidationError::RegionStatsMismatch;
    }
    return HeapValidationError::None;
}

HeapValidationError HeapAllocator::validate_free_list(HeapAllocatorStats & observed) const
{
    const FreeBlock * previous = nullptr;
    uintptr_t previous_end = 0;

    for (const FreeBlock * block = free_head_; block != nullptr; block = block->next)
    {
        const auto block_start = reinterpret_cast<uintptr_t>(block);
        if (block->previous != previous)
        {
            return HeapValidationError::FreeListPreviousMismatch;
        }
        if (previous != nullptr && reinterpret_cast<uintptr_t>(previous) >= block_start)
        {
            return HeapValidationError::FreeListOrder;
        }
        if ((block_start % kMinimumAlignment) != 0)
        {
            return HeapValidationError::FreeBlockMisaligned;
        }
        if (block->size < sizeof(FreeBlock))
        {
            return HeapValidationError::FreeBlockTooSmall;
        }
        if ((block->size % kMinimumAlignment) != 0)
        {
            return HeapValidationError::FreeBlockSizeMisaligned;
        }
        if (block->size > UINTPTR_MAX - block_start ||
            !managed_range(block_start, block->size))
        {
            return HeapValidationError::FreeBlockOutOfRegion;
        }
        if (previous != nullptr && previous_end > block_start)
        {
            return HeapValidationError::FreeBlockOutOfRegion;
        }

        if (observed.free_bytes > static_cast<size_t>(-1) - block->size)
        {
            return HeapValidationError::FreeStatsMismatch;
        }
        observed.free_bytes += block->size;
        ++observed.free_block_count;
        if (block->size > observed.largest_free_block)
        {
            observed.largest_free_block = block->size;
        }

        previous = block;
        previous_end = block_start + block->size;
    }

    if (observed.free_bytes > region_bytes_ || allocated_bytes_ > region_bytes_ ||
        observed.free_bytes > region_bytes_ - allocated_bytes_)
    {
        return HeapValidationError::AllocatedBytesExceedRegion;
    }

    return HeapValidationError::None;
}

void HeapAllocator::insert_free_block(uintptr_t address, size_t bytes)
{
    auto * block = free_block_at(address);
    block->size = bytes;
    block->previous = nullptr;
    block->next = nullptr;

    if (free_head_ == nullptr)
    {
        free_head_ = block;
        return;
    }

    FreeBlock * current = free_head_;
    FreeBlock * previous = nullptr;
    while (current != nullptr && reinterpret_cast<uintptr_t>(current) < address)
    {
        previous = current;
        current = current->next;
    }

    block->previous = previous;
    block->next = current;
    if (previous != nullptr)
    {
        previous->next = block;
    }
    else
    {
        free_head_ = block;
    }
    if (current != nullptr)
    {
        current->previous = block;
    }

    coalesce(*block);
}

void HeapAllocator::remove_free_block(FreeBlock & block)
{
    if (block.previous != nullptr)
    {
        block.previous->next = block.next;
    }
    else
    {
        free_head_ = block.next;
    }

    if (block.next != nullptr)
    {
        block.next->previous = block.previous;
    }
}

void HeapAllocator::replace_free_block(FreeBlock & block, uintptr_t address, size_t bytes)
{
    FreeBlock * previous = block.previous;
    FreeBlock * next = block.next;
    auto * replacement = free_block_at(address);
    replacement->size = bytes;
    replacement->previous = previous;
    replacement->next = next;

    if (previous != nullptr)
    {
        previous->next = replacement;
    }
    else
    {
        free_head_ = replacement;
    }
    if (next != nullptr)
    {
        next->previous = replacement;
    }
}

void HeapAllocator::coalesce(FreeBlock & block)
{
    FreeBlock * current = &block;
    if (current->previous != nullptr)
    {
        const uintptr_t previous_end =
            reinterpret_cast<uintptr_t>(current->previous) + current->previous->size;
        if (previous_end == reinterpret_cast<uintptr_t>(current))
        {
            current->previous->size += current->size;
            current->previous->next = current->next;
            if (current->next != nullptr)
            {
                current->next->previous = current->previous;
            }
            current = current->previous;
        }
    }

    if (current->next != nullptr)
    {
        const uintptr_t current_end = reinterpret_cast<uintptr_t>(current) + current->size;
        if (current_end == reinterpret_cast<uintptr_t>(current->next))
        {
            current->size += current->next->size;
            current->next = current->next->next;
            if (current->next != nullptr)
            {
                current->next->previous = current;
            }
        }
    }
}

} // namespace kernel::memory
