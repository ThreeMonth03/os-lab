#include <gtest/gtest.h>
#include "kernel/memory/memory_map.hpp"
#include "kernel/memory/physical_frame_allocator.hpp"

namespace
{

TEST(MemoryMapViewTest, ClassifiesRegionKinds)
{
    EXPECT_TRUE(kernel::memory::is_allocatable(kernel::memory::MemoryRegionKind::Usable));
    EXPECT_FALSE(
        kernel::memory::is_allocatable(kernel::memory::MemoryRegionKind::BootloaderReclaimable));
    EXPECT_TRUE(
        kernel::memory::is_reclaimable(kernel::memory::MemoryRegionKind::BootloaderReclaimable));
    EXPECT_TRUE(kernel::memory::is_reclaimable(kernel::memory::MemoryRegionKind::AcpiReclaimable));
    EXPECT_TRUE(kernel::memory::is_reserved(kernel::memory::MemoryRegionKind::Framebuffer));
    EXPECT_TRUE(kernel::memory::is_reserved(kernel::memory::MemoryRegionKind::KernelAndModules));
}

TEST(MemoryMapViewTest, SummarizesMemoryByKind)
{
    const kernel::memory::MemoryRegion regions[] = {
        {0x1000, 0x3000, kernel::memory::MemoryRegionKind::Usable},
        {0x4000, 0x1000, kernel::memory::MemoryRegionKind::Reserved},
        {0x5000, 0x2000, kernel::memory::MemoryRegionKind::BootloaderReclaimable},
        {0x7000, 0x4000, kernel::memory::MemoryRegionKind::Framebuffer},
        {0xb000, 0x1000, kernel::memory::MemoryRegionKind::AcpiReclaimable},
    };
    const kernel::memory::MemoryMapStats stats = kernel::memory::MemoryMapView(regions).stats();

    EXPECT_EQ(stats.region_count, 5u);
    EXPECT_EQ(stats.total_bytes, 0xb000u);
    EXPECT_EQ(stats.usable_bytes, 0x3000u);
    EXPECT_EQ(stats.bootloader_reclaimable_bytes, 0x2000u);
    EXPECT_EQ(stats.framebuffer_bytes, 0x4000u);
    EXPECT_EQ(stats.reserved_bytes, 0x2000u);
}

TEST(MemoryMapViewTest, HandlesEmptyMap)
{
    const kernel::memory::MemoryMapStats stats = kernel::memory::MemoryMapView().stats();

    EXPECT_EQ(stats.region_count, 0u);
    EXPECT_EQ(stats.total_bytes, 0u);
    EXPECT_EQ(stats.usable_bytes, 0u);
}

TEST(EarlyFrameAllocatorTest, CountsAlignedFrames)
{
    const kernel::memory::MemoryRegion aligned{0x1000, 0x3000, kernel::memory::MemoryRegionKind::Usable};
    const kernel::memory::MemoryRegion unaligned{0x1800, 0x3800, kernel::memory::MemoryRegionKind::Usable};
    const kernel::memory::MemoryRegion reserved{0x1000, 0x3000, kernel::memory::MemoryRegionKind::Reserved};

    EXPECT_EQ(kernel::memory::usable_frame_count(aligned), 3u);
    EXPECT_EQ(kernel::memory::usable_frame_count(unaligned), 3u);
    EXPECT_EQ(kernel::memory::usable_frame_count(reserved), 0u);
}

TEST(EarlyFrameAllocatorTest, AllocatesAcrossUsableRegionsAndSkipsReserved)
{
    const kernel::memory::MemoryRegion regions[] = {
        {0x1000, 0x2000, kernel::memory::MemoryRegionKind::Usable},
        {0x3000, 0x4000, kernel::memory::MemoryRegionKind::Reserved},
        {0x7000, 0x1000, kernel::memory::MemoryRegionKind::Framebuffer},
        {0x8000, 0x2000, kernel::memory::MemoryRegionKind::Usable},
    };
    kernel::memory::EarlyFrameAllocator allocator{kernel::memory::MemoryMapView(regions)};
    kernel::memory::PhysicalFrame frame;

    EXPECT_EQ(allocator.stats().total_frames, 4u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_TRUE(frame.valid());
    EXPECT_EQ(frame.address(), 0x1000u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address(), 0x2000u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address(), 0x8000u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address(), 0x9000u);
    EXPECT_FALSE(allocator.allocate(frame));
    EXPECT_FALSE(frame.valid());
    EXPECT_EQ(allocator.stats().allocated_frames, 4u);
    EXPECT_EQ(allocator.stats().remaining_frames, 0u);
}

TEST(EarlyFrameAllocatorTest, AlignsAllocationsAndSkipsPhysicalZero)
{
    const kernel::memory::MemoryRegion regions[] = {
        {0x0000, 0x2000, kernel::memory::MemoryRegionKind::Usable},
        {0x2800, 0x1800, kernel::memory::MemoryRegionKind::Usable},
    };
    kernel::memory::EarlyFrameAllocator allocator{kernel::memory::MemoryMapView(regions)};
    kernel::memory::PhysicalFrame frame;

    EXPECT_EQ(allocator.stats().total_frames, 2u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address(), 0x1000u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address(), 0x3000u);
    EXPECT_FALSE(allocator.allocate(frame));
}

TEST(EarlyFrameAllocatorTest, ReportsExhaustionOnEmptyMap)
{
    kernel::memory::EarlyFrameAllocator allocator;
    kernel::memory::PhysicalFrame frame;

    EXPECT_FALSE(allocator.allocate(frame));
    EXPECT_EQ(allocator.stats().total_frames, 0u);
    EXPECT_EQ(allocator.stats().remaining_frames, 0u);
}

} // namespace
