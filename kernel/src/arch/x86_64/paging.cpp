#include "kernel/arch/x86_64/paging.hpp"

namespace kernel::arch::x86_64::paging
{

uint64_t align_down_to_page(uint64_t value) { return value & ~kPageOffsetMask; }

uint64_t align_up_to_page(uint64_t value)
{
    if (is_page_aligned(value))
    {
        return value;
    }

    if (value > UINT64_MAX - kPageOffsetMask)
    {
        return 0;
    }

    return (value + kPageOffsetMask) & ~kPageOffsetMask;
}

bool is_page_aligned(uint64_t value) { return (value & kPageOffsetMask) == 0; }

PageIndices page_indices(uint64_t virtual_address)
{
    return {
        static_cast<uint16_t>((virtual_address >> 39) & kPageTableIndexMask),
        static_cast<uint16_t>((virtual_address >> 30) & kPageTableIndexMask),
        static_cast<uint16_t>((virtual_address >> 21) & kPageTableIndexMask),
        static_cast<uint16_t>((virtual_address >> 12) & kPageTableIndexMask),
        static_cast<uint16_t>(virtual_address & kPageOffsetMask),
    };
}

} // namespace kernel::arch::x86_64::paging
