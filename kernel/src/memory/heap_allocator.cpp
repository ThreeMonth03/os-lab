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
    auto * header = allocation_header_at(payload - sizeof(AllocationHeader));
    if (header->magic != kAllocationMagic || header->block_size < sizeof(FreeBlock))
    {
        return false;
    }

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
