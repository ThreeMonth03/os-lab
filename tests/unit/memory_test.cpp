#include <gtest/gtest.h>
#include <stdint.h>

#include "kernel/memory/heap.hpp"
#include "kernel/memory/heap_allocator.hpp"
#include "kernel/memory/memory_map.hpp"
#include "kernel/memory/physical_frame_allocator.hpp"
#include "kernel/memory/slab_cache.hpp"

namespace
{

constexpr size_t kTestAllocationHeaderBytes =
    sizeof(uint64_t) + sizeof(uintptr_t) + (2 * sizeof(size_t));

void corrupt_allocation_magic(void * memory)
{
    auto * magic = reinterpret_cast<uint64_t *>(
        static_cast<unsigned char *>(memory) - kTestAllocationHeaderBytes);
    *magic = 0;
}

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

TEST(HeapAllocatorTest, AllocatesAlignedBlocksAndReportsStats)
{
    alignas(64) unsigned char storage[512] = {};
    kernel::memory::HeapAllocator allocator;
    allocator.reset(storage, sizeof(storage));

    void * first = allocator.allocate(13, 16);
    void * second = allocator.allocate(32, 64);

    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(first) % 16, 0u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(second) % 64, 0u);

    const kernel::memory::HeapAllocatorStats stats = allocator.stats();
    EXPECT_TRUE(stats.initialized);
    EXPECT_EQ(stats.region_count, 1u);
    EXPECT_EQ(stats.allocated_bytes, 45u);
    EXPECT_EQ(stats.allocation_count, 2u);
    EXPECT_GT(stats.free_bytes, 0u);
    EXPECT_TRUE(allocator.validate().valid);
}

TEST(HeapAllocatorTest, RejectsInvalidAlignmentAndExhaustion)
{
    alignas(16) unsigned char storage[256] = {};
    kernel::memory::HeapAllocator allocator;
    allocator.reset(storage, sizeof(storage));

    EXPECT_NE(allocator.allocate(8, 0), nullptr);
    EXPECT_EQ(allocator.allocate(8, 3), nullptr);
    EXPECT_NE(allocator.allocate(64, 8), nullptr);
    EXPECT_EQ(allocator.allocate(4096, 8), nullptr);
    EXPECT_TRUE(allocator.validate().valid);
}

TEST(HeapAllocatorTest, FreesAndReusesBlocks)
{
    alignas(16) unsigned char storage[256] = {};
    kernel::memory::HeapAllocator allocator;
    allocator.reset(storage, sizeof(storage));

    void * first = allocator.allocate(24, 8);
    void * second = allocator.allocate(24, 8);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    EXPECT_TRUE(allocator.free(first));
    void * reused = allocator.allocate(16, 8);
    EXPECT_EQ(reused, first);
    EXPECT_TRUE(allocator.free(second));
    EXPECT_TRUE(allocator.free(reused));

    const kernel::memory::HeapAllocatorStats stats = allocator.stats();
    EXPECT_EQ(stats.allocation_count, 0u);
    EXPECT_EQ(stats.allocated_bytes, 0u);
    EXPECT_EQ(stats.free_block_count, 1u);
    EXPECT_EQ(stats.free_bytes, stats.region_bytes);
    EXPECT_TRUE(allocator.validate().valid);
}

TEST(HeapAllocatorTest, AddsAdjacentRegionAndCoalesces)
{
    alignas(16) unsigned char storage[512] = {};
    kernel::memory::HeapAllocator allocator;
    allocator.reset(storage, 256);

    EXPECT_TRUE(allocator.add_region(storage + 256, 256));
    const kernel::memory::HeapAllocatorStats stats = allocator.stats();

    EXPECT_EQ(stats.region_bytes, 512u);
    EXPECT_EQ(stats.region_count, 1u);
    EXPECT_EQ(stats.free_block_count, 1u);
    EXPECT_EQ(stats.free_bytes, 512u);
    EXPECT_TRUE(allocator.validate().valid);
}

TEST(HeapAllocatorTest, ValidatesUninitializedAndAllocatedState)
{
    kernel::memory::HeapAllocator allocator;

    kernel::memory::HeapValidationResult validation = allocator.validate();
    EXPECT_TRUE(validation.valid);
    EXPECT_FALSE(validation.observed.initialized);
    EXPECT_EQ(validation.error, kernel::memory::HeapValidationError::None);

    alignas(16) unsigned char storage[256] = {};
    allocator.reset(storage, sizeof(storage));
    void * first = allocator.allocate(24, 8);
    void * second = allocator.allocate(32, 16);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    validation = allocator.validate();
    EXPECT_TRUE(validation.valid);
    EXPECT_TRUE(validation.observed.initialized);
    EXPECT_EQ(validation.observed.allocation_count, 2u);

    EXPECT_TRUE(allocator.free(first));
    EXPECT_TRUE(allocator.free(second));
    validation = allocator.validate();
    EXPECT_TRUE(validation.valid);
    EXPECT_EQ(validation.observed.allocation_count, 0u);
    EXPECT_EQ(validation.observed.free_bytes, validation.observed.region_bytes);
}

TEST(HeapAllocatorTest, RejectsDoubleFreeAndInvalidFree)
{
    alignas(16) unsigned char storage[256] = {};
    kernel::memory::HeapAllocator allocator;
    allocator.reset(storage, sizeof(storage));

    void * memory = allocator.allocate(24, 8);
    ASSERT_NE(memory, nullptr);

    EXPECT_TRUE(allocator.free(memory));
    EXPECT_FALSE(allocator.free(memory));

    int outside_heap = 0;
    EXPECT_FALSE(allocator.free(&outside_heap));
    EXPECT_TRUE(allocator.validate().valid);
}

TEST(HeapAllocatorTest, RejectsInteriorPointerFreeWithoutLosingOriginalBlock)
{
    alignas(16) unsigned char storage[256] = {};
    kernel::memory::HeapAllocator allocator;
    allocator.reset(storage, sizeof(storage));

    void * memory = allocator.allocate(24, 8);
    ASSERT_NE(memory, nullptr);

    auto * interior = static_cast<unsigned char *>(memory) + 1;
    EXPECT_FALSE(allocator.free(interior));
    EXPECT_TRUE(allocator.validate().valid);
    EXPECT_TRUE(allocator.free(memory));
}

TEST(HeapAllocatorTest, RejectsCorruptedAllocationHeader)
{
    alignas(16) unsigned char storage[256] = {};
    kernel::memory::HeapAllocator allocator;
    allocator.reset(storage, sizeof(storage));

    void * memory = allocator.allocate(24, alignof(uintptr_t));
    ASSERT_NE(memory, nullptr);
    corrupt_allocation_magic(memory);

    EXPECT_FALSE(allocator.free(memory));
    EXPECT_TRUE(allocator.validate().valid);
}

TEST(HeapRangeTest, CountsRequiredPages)
{
    EXPECT_EQ(kernel::memory::heap::page_count_for_bytes(0), 0u);
    EXPECT_EQ(kernel::memory::heap::page_count_for_bytes(1), 1u);
    EXPECT_EQ(kernel::memory::heap::page_count_for_bytes(kernel::arch::x86_64::paging::kPageSize), 1u);
    EXPECT_EQ(kernel::memory::heap::page_count_for_bytes(kernel::arch::x86_64::paging::kPageSize + 1), 2u);
}

TEST(HeapRangeTest, ChecksBounds)
{
    EXPECT_TRUE(kernel::memory::heap::contains(kernel::memory::heap::kVirtualBase));
    EXPECT_TRUE(kernel::memory::heap::contains(kernel::memory::heap::kVirtualBase +
                                               kernel::memory::heap::kMaxBytes - 1));
    EXPECT_FALSE(kernel::memory::heap::contains(kernel::memory::heap::kVirtualBase - 1));
    EXPECT_FALSE(kernel::memory::heap::contains(kernel::memory::heap::kVirtualBase +
                                                kernel::memory::heap::kMaxBytes));
    EXPECT_TRUE(kernel::memory::heap::contains(kernel::memory::heap::kVirtualBase, 4096));
    EXPECT_FALSE(kernel::memory::heap::contains(kernel::memory::heap::kVirtualBase +
                                                    kernel::memory::heap::kMaxBytes - 2048,
                                                4096));
}

TEST(SlabCacheTest, RejectsInvalidObjectSizeAndAlignment)
{
    kernel::memory::SlabCache cache;

    EXPECT_FALSE(cache.init(0, 8));
    EXPECT_FALSE(cache.initialized());
    EXPECT_FALSE(cache.init(16, 3));
    EXPECT_FALSE(cache.initialized());
    EXPECT_TRUE(cache.validate().valid);
}

TEST(SlabCacheTest, AllocatesAlignedObjectsAndReportsStats)
{
    alignas(64) unsigned char storage[512] = {};
    kernel::memory::SlabCache cache;

    ASSERT_TRUE(cache.init(24, 32));
    ASSERT_TRUE(cache.add_slab(storage, sizeof(storage)));

    void * first = cache.allocate();
    void * second = cache.allocate();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(first) % 32, 0u);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(second) % 32, 0u);

    const kernel::memory::SlabCacheStats stats = cache.stats();
    EXPECT_TRUE(stats.initialized);
    EXPECT_EQ(stats.object_size, 24u);
    EXPECT_EQ(stats.alignment, 32u);
    EXPECT_EQ(stats.slab_count, 1u);
    EXPECT_EQ(stats.allocated_objects, 2u);
    EXPECT_EQ(stats.object_capacity, stats.allocated_objects + stats.free_objects);
    EXPECT_TRUE(cache.validate().valid);
}

TEST(SlabCacheTest, ReportsExhaustionAndFailedAllocations)
{
    alignas(16) unsigned char storage[144] = {};
    kernel::memory::SlabCache cache;

    ASSERT_TRUE(cache.init(64, 16));
    ASSERT_TRUE(cache.add_slab(storage, sizeof(storage)));

    void * first = cache.allocate();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(cache.allocate(), nullptr);

    const kernel::memory::SlabCacheStats stats = cache.stats();
    EXPECT_EQ(stats.allocated_objects, 1u);
    EXPECT_EQ(stats.free_objects, 0u);
    EXPECT_EQ(stats.failed_allocations, 1u);
    EXPECT_TRUE(cache.validate().valid);
}

TEST(SlabCacheTest, FreesAndReusesObjects)
{
    alignas(16) unsigned char storage[256] = {};
    kernel::memory::SlabCache cache;

    ASSERT_TRUE(cache.init(16, 16));
    ASSERT_TRUE(cache.add_slab(storage, sizeof(storage)));

    void * first = cache.allocate();
    void * second = cache.allocate();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);

    EXPECT_TRUE(cache.free(first));
    void * reused = cache.allocate();
    EXPECT_EQ(reused, first);
    EXPECT_TRUE(cache.free(second));
    EXPECT_TRUE(cache.free(reused));

    const kernel::memory::SlabCacheStats stats = cache.stats();
    EXPECT_EQ(stats.allocated_objects, 0u);
    EXPECT_EQ(stats.free_objects, stats.object_capacity);
    EXPECT_TRUE(cache.validate().valid);
}

TEST(SlabCacheTest, RejectsDoubleFreeAndInvalidFree)
{
    alignas(16) unsigned char storage[256] = {};
    kernel::memory::SlabCache cache;

    ASSERT_TRUE(cache.init(16, 16));
    ASSERT_TRUE(cache.add_slab(storage, sizeof(storage)));

    void * object = cache.allocate();
    ASSERT_NE(object, nullptr);

    EXPECT_TRUE(cache.free(object));
    EXPECT_FALSE(cache.free(object));

    int outside_cache = 0;
    EXPECT_FALSE(cache.free(&outside_cache));
    EXPECT_TRUE(cache.validate().valid);
}

TEST(SlabCacheTest, RejectsInteriorPointerFree)
{
    alignas(16) unsigned char storage[256] = {};
    kernel::memory::SlabCache cache;

    ASSERT_TRUE(cache.init(16, 16));
    ASSERT_TRUE(cache.add_slab(storage, sizeof(storage)));

    void * object = cache.allocate();
    ASSERT_NE(object, nullptr);

    auto * interior = static_cast<unsigned char *>(object) + 1;
    EXPECT_FALSE(cache.free(interior));
    EXPECT_TRUE(cache.validate().valid);
    EXPECT_TRUE(cache.free(object));
}

TEST(SlabCacheTest, ValidatesAfterMultipleSlabs)
{
    alignas(16) unsigned char first_storage[256] = {};
    alignas(16) unsigned char second_storage[256] = {};
    kernel::memory::SlabCache cache;

    ASSERT_TRUE(cache.init(16, 16));
    ASSERT_TRUE(cache.add_slab(first_storage, sizeof(first_storage)));
    ASSERT_TRUE(cache.add_slab(second_storage, sizeof(second_storage)));

    void * first = cache.allocate();
    void * second = cache.allocate();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_TRUE(cache.free(first));

    const kernel::memory::SlabValidationResult validation = cache.validate();
    EXPECT_TRUE(validation.valid);
    EXPECT_EQ(validation.observed.slab_count, 2u);
    EXPECT_EQ(validation.observed.allocated_objects, 1u);
}

} // namespace
