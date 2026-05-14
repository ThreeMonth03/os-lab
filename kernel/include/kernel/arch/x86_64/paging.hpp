#pragma once

#include <stdint.h>

namespace kernel::arch::x86_64::paging
{

inline constexpr uint64_t kPageSize = 4096;
inline constexpr uint64_t kPageOffsetMask = kPageSize - 1;
inline constexpr uint64_t kPageAddressMask = 0x000ffffffffff000;
inline constexpr uint16_t kPageTableIndexMask = 0x01ff;

enum class PageFlag : uint64_t
{
    Present = 1ull << 0,
    Writable = 1ull << 1,
    User = 1ull << 2,
    WriteThrough = 1ull << 3,
    NoCache = 1ull << 4,
    Accessed = 1ull << 5,
    Dirty = 1ull << 6,
    HugePage = 1ull << 7,
    Global = 1ull << 8,
    NoExecute = 1ull << 63,
};

class PageFlags
{
public:
    constexpr PageFlags() = default;
    constexpr PageFlags(PageFlag flag)
        : bits_(static_cast<uint64_t>(flag))
    {
    }

    [[nodiscard]] static constexpr PageFlags from_bits(uint64_t bits) { return PageFlags(bits); }

    [[nodiscard]] constexpr uint64_t bits() const { return bits_; }
    [[nodiscard]] constexpr bool contains(PageFlag flag) const
    {
        return (bits_ & static_cast<uint64_t>(flag)) != 0;
    }

    [[nodiscard]] constexpr PageFlags with(PageFlag flag) const
    {
        return PageFlags(bits_ | static_cast<uint64_t>(flag));
    }

private:
    explicit constexpr PageFlags(uint64_t bits)
        : bits_(bits)
    {
    }

    uint64_t bits_ = 0;
};

[[nodiscard]] constexpr PageFlags operator|(PageFlag left, PageFlag right)
{
    return PageFlags(left).with(right);
}

[[nodiscard]] constexpr PageFlags operator|(PageFlags left, PageFlag right)
{
    return left.with(right);
}

[[nodiscard]] constexpr PageFlags operator|(PageFlags left, PageFlags right)
{
    return PageFlags::from_bits(left.bits() | right.bits());
}

[[nodiscard]] constexpr PageFlags operator|(PageFlag left, PageFlags right)
{
    return PageFlags(left) | right;
}

struct PageIndices
{
    uint16_t pml4 = 0;
    uint16_t pdpt = 0;
    uint16_t pd = 0;
    uint16_t pt = 0;
    uint16_t offset = 0;
};

[[nodiscard]] uint64_t align_down_to_page(uint64_t value);
[[nodiscard]] uint64_t align_up_to_page(uint64_t value);
[[nodiscard]] bool is_page_aligned(uint64_t value);
[[nodiscard]] PageIndices page_indices(uint64_t virtual_address);

} // namespace kernel::arch::x86_64::paging
