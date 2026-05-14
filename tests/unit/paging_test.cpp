#include <gtest/gtest.h>

#include <cstddef>

#include "kernel/arch/x86_64/paging.hpp"

namespace
{

namespace paging = kernel::arch::x86_64::paging;

constexpr size_t kTestPageTableCount = 8;
constexpr uint64_t kTestPageTableBase = 0x100000;

struct TestPageTableArena
{
    paging::PageTable tables[kTestPageTableCount] = {};
    uint64_t physical_addresses[kTestPageTableCount] = {};
    size_t allocated = 0;
};

bool allocate_test_table(paging::PageTable *& table, uint64_t & physical_address, void * context)
{
    auto & arena = *static_cast<TestPageTableArena *>(context);
    if (arena.allocated >= 8)
    {
        table = nullptr;
        physical_address = 0;
        return false;
    }

    table = &arena.tables[arena.allocated++];
    for (paging::PageTableEntry & entry : table->entries)
    {
        entry.clear();
    }

    const size_t table_index = arena.allocated - 1;
    physical_address = kTestPageTableBase + (table_index * paging::kPageSize);
    arena.physical_addresses[table_index] = physical_address;
    return true;
}

paging::PageTable * test_table_from_physical(uint64_t physical_address, void * context)
{
    auto & arena = *static_cast<TestPageTableArena *>(context);
    for (size_t index = 0; index < arena.allocated; ++index)
    {
        if (arena.physical_addresses[index] == physical_address)
        {
            return &arena.tables[index];
        }
    }
    return nullptr;
}

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

TEST(PageTableEntryTest, StoresPhysicalAddressAndFlags)
{
    paging::PageTableEntry entry;
    entry.set(0x12345000, paging::PageFlag::Writable | paging::PageFlag::NoExecute);

    EXPECT_TRUE(entry.present());
    EXPECT_EQ(entry.address(), 0x12345000u);
    EXPECT_TRUE(entry.flags().contains(paging::PageFlag::Present));
    EXPECT_TRUE(entry.flags().contains(paging::PageFlag::Writable));
    EXPECT_TRUE(entry.flags().contains(paging::PageFlag::NoExecute));

    entry.clear();
    EXPECT_FALSE(entry.present());
}

TEST(PageTableManagerTest, MapsAndTranslatesPage)
{
    alignas(paging::kPageSize) paging::PageTable root = {};
    TestPageTableArena arena;
    paging::PageTableManager manager(root, allocate_test_table, test_table_from_physical, &arena);

    const uint64_t virtual_address = 0xffff800000401000;
    const uint64_t physical_address = 0x00203000;

    EXPECT_EQ(manager.map_page(virtual_address,
                               physical_address,
                               paging::PageFlag::Writable | paging::PageFlag::NoExecute),
              paging::MapResult::Mapped);
    EXPECT_EQ(arena.allocated, 3u);

    paging::Translation translation;
    ASSERT_TRUE(manager.translate(virtual_address + 0x123, translation));
    EXPECT_EQ(translation.physical_address, physical_address + 0x123);
    EXPECT_TRUE(translation.flags.contains(paging::PageFlag::Present));
    EXPECT_TRUE(translation.flags.contains(paging::PageFlag::Writable));
    EXPECT_TRUE(translation.flags.contains(paging::PageFlag::NoExecute));
}

TEST(PageTableManagerTest, RejectsUnalignedAndDuplicateMappings)
{
    alignas(paging::kPageSize) paging::PageTable root = {};
    TestPageTableArena arena;
    paging::PageTableManager manager(root, allocate_test_table, test_table_from_physical, &arena);

    EXPECT_EQ(manager.map_page(0x1001, 0x2000, paging::PageFlag::Writable),
              paging::MapResult::InvalidAlignment);
    EXPECT_EQ(manager.map_page(0x1000, 0x2001, paging::PageFlag::Writable),
              paging::MapResult::InvalidAlignment);
    EXPECT_EQ(manager.map_page(0x1000, 0x2000, paging::PageFlag::Writable),
              paging::MapResult::Mapped);
    EXPECT_EQ(manager.map_page(0x1000, 0x3000, paging::PageFlag::Writable),
              paging::MapResult::AlreadyMapped);
}

TEST(PageTableManagerTest, ReportsMissingMappings)
{
    alignas(paging::kPageSize) paging::PageTable root = {};
    TestPageTableArena arena;
    paging::PageTableManager manager(root, allocate_test_table, test_table_from_physical, &arena);
    paging::Translation translation;

    EXPECT_FALSE(manager.translate(0x4000, translation));
    EXPECT_EQ(translation.physical_address, 0u);
}

} // namespace
