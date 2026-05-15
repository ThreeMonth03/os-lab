#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel::memory
{

struct SlabCacheStats
{
    bool initialized = false;
    size_t object_size = 0;
    size_t alignment = 0;
    size_t object_capacity = 0;
    size_t slab_count = 0;
    size_t committed_bytes = 0;
    size_t allocated_objects = 0;
    size_t free_objects = 0;
    size_t failed_allocations = 0;
};

enum class SlabValidationError
{
    None,
    InvalidConfig,
    SlabHeaderCorrupt,
    SlabOutOfRange,
    SlabOverlap,
    FreeListOutOfSlab,
    FreeListDuplicate,
    FreeStatsMismatch,
    StatsMismatch,
};

struct SlabValidationResult
{
    bool valid = true;
    SlabValidationError error = SlabValidationError::None;
    SlabCacheStats observed;
};

class SlabCache
{
public:
    SlabCache() = default;

    [[nodiscard]] bool init(size_t object_size, size_t alignment);
    [[nodiscard]] bool add_slab(void * memory, size_t bytes);

    [[nodiscard]] void * allocate();
    [[nodiscard]] bool free(void * memory);

    SlabCacheStats stats() const;
    [[nodiscard]] SlabValidationResult validate() const;
    bool initialized() const { return initialized_; }

private:
    struct FreeNode;
    struct SlabHeader;

    [[nodiscard]] static bool is_power_of_two(size_t value);
    [[nodiscard]] static size_t normalize_alignment(size_t alignment);
    [[nodiscard]] static uintptr_t align_up(uintptr_t value, size_t alignment);
    [[nodiscard]] static FreeNode * free_node_at(uintptr_t address);
    [[nodiscard]] static SlabHeader * slab_header_at(uintptr_t address);

    [[nodiscard]] bool calculate_layout(void * memory,
                                        size_t bytes,
                                        uintptr_t & header_address,
                                        uintptr_t & object_start,
                                        size_t & object_count,
                                        size_t & committed_bytes) const;
    [[nodiscard]] SlabHeader * find_slab(uintptr_t address, size_t & object_index) const;
    [[nodiscard]] static bool object_on_free_list(const SlabHeader & slab, uintptr_t address);
    [[nodiscard]] bool slab_ranges_overlap(const SlabHeader & current) const;
    [[nodiscard]] SlabValidationError validate_slab(const SlabHeader & slab,
                                                    SlabCacheStats & observed) const;

    SlabHeader * slabs_ = nullptr;
    size_t object_size_ = 0;
    size_t alignment_ = 0;
    size_t stride_ = 0;
    size_t slab_count_ = 0;
    size_t committed_bytes_ = 0;
    size_t object_capacity_ = 0;
    size_t allocated_objects_ = 0;
    size_t free_objects_ = 0;
    size_t failed_allocations_ = 0;
    bool initialized_ = false;
};

} // namespace kernel::memory
