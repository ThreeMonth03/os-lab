#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/arch/x86_64/paging.hpp"
#include "kernel/memory/heap_allocator.hpp"

namespace kernel::memory::heap
{

inline constexpr uint64_t kVirtualBase = 0xffff900100000000;
inline constexpr size_t kMaxBytes = 16 * 1024 * 1024;
inline constexpr size_t kInitialCommitPages = 4;

struct Stats
{
    bool initialized = false;
    uint64_t virtual_base = kVirtualBase;
    size_t reserved_bytes = kMaxBytes;
    size_t committed_bytes = 0;
    size_t committed_pages = 0;
    size_t failed_allocations = 0;
    HeapAllocatorStats allocator;
};

[[nodiscard]] constexpr size_t page_count_for_bytes(size_t bytes)
{
    if (bytes == 0)
    {
        return 0;
    }

    const size_t page_size = kernel::arch::x86_64::paging::kPageSize;
    if (bytes > static_cast<size_t>(-1) - (page_size - 1))
    {
        return 0;
    }

    return (bytes + page_size - 1) / page_size;
}

[[nodiscard]] constexpr bool contains(uint64_t address, size_t bytes = 1)
{
    if (bytes == 0)
    {
        return address >= kVirtualBase && address <= kVirtualBase + kMaxBytes;
    }

    if (address < kVirtualBase)
    {
        return false;
    }

    const uint64_t offset = address - kVirtualBase;
    return offset < kMaxBytes && bytes <= kMaxBytes - offset;
}

[[nodiscard]] bool init();
[[nodiscard]] void * allocate(size_t bytes, size_t alignment);
[[nodiscard]] bool free(void * memory);
[[nodiscard]] Stats stats();
[[nodiscard]] HeapValidationResult validate();

} // namespace kernel::memory::heap
