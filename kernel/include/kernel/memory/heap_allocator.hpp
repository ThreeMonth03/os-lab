#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel::memory
{

struct HeapAllocatorStats
{
    bool initialized = false;
    size_t region_count = 0;
    size_t region_bytes = 0;
    size_t allocated_bytes = 0;
    size_t allocation_count = 0;
    size_t free_bytes = 0;
    size_t free_block_count = 0;
    size_t largest_free_block = 0;
};

enum class HeapValidationError
{
    None,
    RegionListFull,
    RegionMisaligned,
    RegionTooSmall,
    RegionOverlap,
    RegionStatsMismatch,
    FreeListPreviousMismatch,
    FreeListOrder,
    FreeBlockMisaligned,
    FreeBlockTooSmall,
    FreeBlockSizeMisaligned,
    FreeBlockOutOfRegion,
    FreeStatsMismatch,
    AllocatedBytesExceedRegion,
};

struct HeapValidationResult
{
    bool valid = true;
    HeapValidationError error = HeapValidationError::None;
    HeapAllocatorStats observed;
};

class HeapAllocator
{
public:
    HeapAllocator() = default;

    void reset(void * memory, size_t bytes);
    [[nodiscard]] bool add_region(void * memory, size_t bytes);

    [[nodiscard]] void * allocate(size_t bytes, size_t alignment);
    [[nodiscard]] bool free(void * memory);

    HeapAllocatorStats stats() const;
    [[nodiscard]] HeapValidationResult validate() const;
    bool initialized() const { return initialized_; }

private:
    static constexpr size_t kMaxRegions = 64;

    struct Region
    {
        uintptr_t start = 0;
        size_t size = 0;
    };
    struct FreeBlock;

    [[nodiscard]] static size_t normalize_alignment(size_t alignment);
    [[nodiscard]] static bool is_power_of_two(size_t value);
    [[nodiscard]] static uintptr_t align_up(uintptr_t value, size_t alignment);

    [[nodiscard]] static bool usable_region(void * memory,
                                            size_t bytes,
                                            uintptr_t & start,
                                            size_t & usable_bytes);
    [[nodiscard]] static FreeBlock * free_block_at(uintptr_t address);
    [[nodiscard]] bool record_region(uintptr_t start, size_t bytes);
    [[nodiscard]] bool managed_range(uintptr_t address, size_t bytes) const;
    [[nodiscard]] bool overlaps_free_block(uintptr_t start, size_t bytes) const;
    [[nodiscard]] bool allocation_header_valid(uintptr_t payload) const;
    [[nodiscard]] HeapValidationError validate_regions(size_t & bytes) const;
    [[nodiscard]] HeapValidationError validate_free_list(HeapAllocatorStats & observed) const;
    void insert_free_block(uintptr_t address, size_t bytes);
    void remove_free_block(FreeBlock & block);
    void replace_free_block(FreeBlock & block, uintptr_t address, size_t bytes);
    static void coalesce(FreeBlock & block);

    Region regions_[kMaxRegions] = {};
    size_t region_count_ = 0;
    FreeBlock * free_head_ = nullptr;
    size_t region_bytes_ = 0;
    size_t allocated_bytes_ = 0;
    size_t allocation_count_ = 0;
    bool initialized_ = false;
};

} // namespace kernel::memory
