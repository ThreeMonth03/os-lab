#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel::memory
{

struct HeapAllocatorStats
{
    bool initialized = false;
    size_t region_bytes = 0;
    size_t allocated_bytes = 0;
    size_t allocation_count = 0;
    size_t free_bytes = 0;
    size_t free_block_count = 0;
    size_t largest_free_block = 0;
};

class HeapAllocator
{
public:
    HeapAllocator() = default;

    void reset(void * memory, size_t bytes);
    [[nodiscard]] bool add_region(void * memory, size_t bytes);

    [[nodiscard]] void * allocate(size_t bytes, size_t alignment);
    [[nodiscard]] bool free(void * memory);

    [[nodiscard]] HeapAllocatorStats stats() const;
    [[nodiscard]] bool initialized() const { return initialized_; }

private:
    struct FreeBlock;

    [[nodiscard]] static size_t normalize_alignment(size_t alignment);
    [[nodiscard]] static bool is_power_of_two(size_t value);
    [[nodiscard]] static uintptr_t align_up(uintptr_t value, size_t alignment);

    [[nodiscard]] static bool usable_region(void * memory,
                                            size_t bytes,
                                            uintptr_t & start,
                                            size_t & usable_bytes);
    [[nodiscard]] static FreeBlock * free_block_at(uintptr_t address);
    void insert_free_block(uintptr_t address, size_t bytes);
    void remove_free_block(FreeBlock & block);
    void replace_free_block(FreeBlock & block, uintptr_t address, size_t bytes);
    static void coalesce(FreeBlock & block);

    FreeBlock * free_head_ = nullptr;
    size_t region_bytes_ = 0;
    size_t allocated_bytes_ = 0;
    size_t allocation_count_ = 0;
    bool initialized_ = false;
};

} // namespace kernel::memory
