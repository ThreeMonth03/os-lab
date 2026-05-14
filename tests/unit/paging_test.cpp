#include <gtest/gtest.h>
#include "kernel/arch/x86_64/paging.hpp"

namespace
{

namespace paging = kernel::arch::x86_64::paging;

TEST(PagingAddressTest, AlignsAddressesToPageBoundaries)
{
    EXPECT_TRUE(paging::is_page_aligned(0));
    EXPECT_TRUE(paging::is_page_aligned(0x2000));
    EXPECT_FALSE(paging::is_page_aligned(0x2001));

    EXPECT_EQ(paging::align_down_to_page(0x2345), 0x2000u);
    EXPECT_EQ(paging::align_up_to_page(0x2000), 0x2000u);
    EXPECT_EQ(paging::align_up_to_page(0x2001), 0x3000u);
    EXPECT_EQ(paging::align_up_to_page(UINT64_MAX), 0u);
}

TEST(PagingAddressTest, SplitsVirtualAddressIntoPageTableIndices)
{
    const uint64_t address = (1ull << 39) | (2ull << 30) | (3ull << 21) | (4ull << 12) | 0x0abc;
    const paging::PageIndices indices = paging::page_indices(address);

    EXPECT_EQ(indices.pml4, 1u);
    EXPECT_EQ(indices.pdpt, 2u);
    EXPECT_EQ(indices.pd, 3u);
    EXPECT_EQ(indices.pt, 4u);
    EXPECT_EQ(indices.offset, 0x0abcu);
}

TEST(PagingFlagsTest, EncodesAndQueriesFlags)
{
    const paging::PageFlags flags =
        paging::PageFlag::Present | paging::PageFlag::Writable | paging::PageFlag::NoExecute;

    EXPECT_TRUE(flags.contains(paging::PageFlag::Present));
    EXPECT_TRUE(flags.contains(paging::PageFlag::Writable));
    EXPECT_TRUE(flags.contains(paging::PageFlag::NoExecute));
    EXPECT_FALSE(flags.contains(paging::PageFlag::User));
    EXPECT_EQ(flags.bits(), (1ull << 0) | (1ull << 1) | (1ull << 63));
}

} // namespace
