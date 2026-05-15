#include "kernel/memory/slab_cache.hpp"

namespace
{

constexpr uint64_t kSlabMagic = 0x6f736c6162534c42ull;
constexpr size_t kMinimumAlignment = alignof(uintptr_t);

} // namespace

namespace kernel::memory
{

struct SlabCache::FreeNode
{
    FreeNode * next = nullptr;
};

struct SlabCache::SlabHeader
{
    uint64_t magic = 0;
    SlabHeader * next = nullptr;
    uintptr_t slab_start = 0;
    size_t slab_bytes = 0;
    uintptr_t object_start = 0;
    size_t object_count = 0;
    size_t free_count = 0;
    FreeNode * free_head = nullptr;
};

bool SlabCache::init(size_t object_size, size_t alignment)
{
    slabs_ = nullptr;
    object_size_ = 0;
    alignment_ = 0;
    stride_ = 0;
    slab_count_ = 0;
    committed_bytes_ = 0;
    object_capacity_ = 0;
    allocated_objects_ = 0;
    free_objects_ = 0;
    failed_allocations_ = 0;
    initialized_ = false;

    if (object_size == 0)
    {
        return false;
    }
    if (alignment == 0)
    {
        alignment = kMinimumAlignment;
    }
    else if (!is_power_of_two(alignment))
    {
        return false;
    }

    alignment = normalize_alignment(alignment);
    const size_t minimum_object_bytes =
        object_size < sizeof(FreeNode) ? sizeof(FreeNode) : object_size;
    if (minimum_object_bytes > static_cast<size_t>(-1) - (alignment - 1))
    {
        return false;
    }

    object_size_ = object_size;
    alignment_ = alignment;
    stride_ = align_up(minimum_object_bytes, alignment_);
    initialized_ = true;
    return true;
}

bool SlabCache::add_slab(void * memory, size_t bytes)
{
    if (!initialized_)
    {
        return false;
    }

    uintptr_t header_address = 0;
    uintptr_t object_start = 0;
    size_t object_count = 0;
    size_t committed_bytes = 0;
    if (!calculate_layout(memory, bytes, header_address, object_start, object_count, committed_bytes))
    {
        return false;
    }

    auto * header = slab_header_at(header_address);
    header->magic = kSlabMagic;
    header->next = slabs_;
    header->slab_start = header_address;
    header->slab_bytes = committed_bytes;
    header->object_start = object_start;
    header->object_count = object_count;
    header->free_count = object_count;
    header->free_head = nullptr;

    for (size_t index = 0; index < object_count; ++index)
    {
        const uintptr_t object_address = object_start + (index * stride_);
        auto * node = free_node_at(object_address);
        node->next = header->free_head;
        header->free_head = node;
    }

    slabs_ = header;
    ++slab_count_;
    committed_bytes_ += committed_bytes;
    object_capacity_ += object_count;
    free_objects_ += object_count;
    return true;
}

void * SlabCache::allocate()
{
    if (!initialized_)
    {
        ++failed_allocations_;
        return nullptr;
    }

    for (SlabHeader * slab = slabs_; slab != nullptr; slab = slab->next)
    {
        if (slab->free_head == nullptr)
        {
            continue;
        }

        FreeNode * node = slab->free_head;
        slab->free_head = node->next;
        --slab->free_count;
        --free_objects_;
        ++allocated_objects_;
        return node;
    }

    ++failed_allocations_;
    return nullptr;
}

bool SlabCache::free(void * memory)
{
    if (memory == nullptr)
    {
        return true;
    }
    if (!initialized_)
    {
        return false;
    }

    const auto address = reinterpret_cast<uintptr_t>(memory);
    size_t object_index = 0;
    SlabHeader * slab = find_slab(address, object_index);
    if (slab == nullptr || object_index >= slab->object_count || object_on_free_list(*slab, address))
    {
        return false;
    }

    auto * node = free_node_at(address);
    node->next = slab->free_head;
    slab->free_head = node;
    ++slab->free_count;
    ++free_objects_;
    --allocated_objects_;
    return true;
}

SlabCacheStats SlabCache::stats() const
{
    return {
        initialized_,
        object_size_,
        alignment_,
        object_capacity_,
        slab_count_,
        committed_bytes_,
        allocated_objects_,
        free_objects_,
        failed_allocations_,
    };
}

SlabValidationResult SlabCache::validate() const
{
    SlabValidationResult result;
    result.observed.initialized = initialized_;
    result.observed.object_size = object_size_;
    result.observed.alignment = alignment_;
    result.observed.failed_allocations = failed_allocations_;

    if (!initialized_)
    {
        return result;
    }
    if (object_size_ == 0 || !is_power_of_two(alignment_) || stride_ == 0 ||
        (stride_ % alignment_) != 0 || stride_ < sizeof(FreeNode))
    {
        result.valid = false;
        result.error = SlabValidationError::InvalidConfig;
        return result;
    }

    for (const SlabHeader * slab = slabs_; slab != nullptr; slab = slab->next)
    {
        result.error = validate_slab(*slab, result.observed);
        if (result.error != SlabValidationError::None)
        {
            result.valid = false;
            return result;
        }
    }

    result.observed.allocated_objects =
        result.observed.object_capacity - result.observed.free_objects;
    if (result.observed.slab_count != slab_count_ ||
        result.observed.committed_bytes != committed_bytes_ ||
        result.observed.object_capacity != object_capacity_ ||
        result.observed.free_objects != free_objects_ ||
        result.observed.allocated_objects != allocated_objects_)
    {
        result.valid = false;
        result.error = SlabValidationError::StatsMismatch;
    }
    return result;
}

bool SlabCache::is_power_of_two(size_t value)
{
    return value != 0 && (value & (value - 1)) == 0;
}

size_t SlabCache::normalize_alignment(size_t alignment)
{
    return alignment < kMinimumAlignment ? kMinimumAlignment : alignment;
}

uintptr_t SlabCache::align_up(uintptr_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(static_cast<uintptr_t>(alignment) - 1);
}

SlabCache::FreeNode * SlabCache::free_node_at(uintptr_t address)
{
    return reinterpret_cast<FreeNode *>(address); // NOLINT(performance-no-int-to-ptr)
}

SlabCache::SlabHeader * SlabCache::slab_header_at(uintptr_t address)
{
    return reinterpret_cast<SlabHeader *>(address); // NOLINT(performance-no-int-to-ptr)
}

bool SlabCache::calculate_layout(void * memory,
                                 size_t bytes,
                                 uintptr_t & header_address,
                                 uintptr_t & object_start,
                                 size_t & object_count,
                                 size_t & committed_bytes) const
{
    header_address = 0;
    object_start = 0;
    object_count = 0;
    committed_bytes = 0;
    if (memory == nullptr || bytes < sizeof(SlabHeader))
    {
        return false;
    }

    const auto raw_start = reinterpret_cast<uintptr_t>(memory);
    if (bytes > UINTPTR_MAX - raw_start)
    {
        return false;
    }
    const uintptr_t raw_end = raw_start + bytes;
    header_address = align_up(raw_start, alignof(SlabHeader));
    if (header_address > raw_end || sizeof(SlabHeader) > raw_end - header_address)
    {
        return false;
    }

    const uintptr_t header_end = header_address + sizeof(SlabHeader);
    object_start = align_up(header_end, alignment_);
    if (object_start > raw_end)
    {
        return false;
    }

    const size_t available = raw_end - object_start;
    object_count = available / stride_;
    if (object_count == 0)
    {
        return false;
    }

    committed_bytes = raw_end - header_address;
    return true;
}

SlabCache::SlabHeader * SlabCache::find_slab(uintptr_t address, size_t & object_index) const
{
    object_index = 0;
    for (SlabHeader * slab = slabs_; slab != nullptr; slab = slab->next)
    {
        const uintptr_t object_end = slab->object_start + (slab->object_count * stride_);
        if (address < slab->object_start || address >= object_end)
        {
            continue;
        }

        const uintptr_t offset = address - slab->object_start;
        if ((offset % stride_) != 0)
        {
            return nullptr;
        }

        object_index = offset / stride_;
        return slab;
    }
    return nullptr;
}

bool SlabCache::object_on_free_list(const SlabHeader & slab, uintptr_t address)
{
    for (const FreeNode * node = slab.free_head; node != nullptr; node = node->next)
    {
        if (reinterpret_cast<uintptr_t>(node) == address)
        {
            return true;
        }
    }
    return false;
}

bool SlabCache::slab_ranges_overlap(const SlabHeader & current) const
{
    const uintptr_t current_end = current.slab_start + current.slab_bytes;
    for (const SlabHeader * slab = slabs_; slab != nullptr; slab = slab->next)
    {
        if (slab == &current)
        {
            continue;
        }

        const uintptr_t slab_end = slab->slab_start + slab->slab_bytes;
        if (current.slab_start < slab_end && current_end > slab->slab_start)
        {
            return true;
        }
    }
    return false;
}

SlabValidationError SlabCache::validate_slab(const SlabHeader & slab,
                                             SlabCacheStats & observed) const
{
    if (slab.magic != kSlabMagic)
    {
        return SlabValidationError::SlabHeaderCorrupt;
    }
    if (slab.slab_bytes < sizeof(SlabHeader) || slab.slab_bytes > UINTPTR_MAX - slab.slab_start ||
        slab.object_count == 0 || slab.free_count > slab.object_count)
    {
        return SlabValidationError::SlabOutOfRange;
    }

    const uintptr_t slab_end = slab.slab_start + slab.slab_bytes;
    const uintptr_t object_bytes = slab.object_count * stride_;
    if ((slab.object_start % alignment_) != 0 || slab.object_start < slab.slab_start ||
        slab.object_start > slab_end || object_bytes > slab_end - slab.object_start)
    {
        return SlabValidationError::SlabOutOfRange;
    }
    if (slab_ranges_overlap(slab))
    {
        return SlabValidationError::SlabOverlap;
    }

    size_t free_count = 0;
    for (const FreeNode * node = slab.free_head; node != nullptr; node = node->next)
    {
        if (free_count >= slab.object_count)
        {
            return SlabValidationError::FreeListDuplicate;
        }

        const auto node_address = reinterpret_cast<uintptr_t>(node);
        size_t object_index = 0;
        const SlabHeader * owner = find_slab(node_address, object_index);
        if (owner != &slab || object_index >= slab.object_count)
        {
            return SlabValidationError::FreeListOutOfSlab;
        }

        size_t seen_index = 0;
        for (const FreeNode * seen = slab.free_head; seen != node; seen = seen->next)
        {
            if (seen_index >= slab.object_count || seen == node)
            {
                return SlabValidationError::FreeListDuplicate;
            }
            if (reinterpret_cast<uintptr_t>(seen) == node_address)
            {
                return SlabValidationError::FreeListDuplicate;
            }
            ++seen_index;
        }

        ++free_count;
    }

    if (free_count != slab.free_count)
    {
        return SlabValidationError::FreeStatsMismatch;
    }

    ++observed.slab_count;
    observed.committed_bytes += slab.slab_bytes;
    observed.object_capacity += slab.object_count;
    observed.free_objects += free_count;
    return SlabValidationError::None;
}

} // namespace kernel::memory
