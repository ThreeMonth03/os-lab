#include "kernel/memory/memory.hpp"

#include <limine.h>

#include "kernel/boot/limine_support.hpp"

namespace
{

kernel::memory::MemoryRegionKind memory_region_kind_from_limine(uint64_t type)
{
    switch (type)
    {
    case LIMINE_MEMMAP_USABLE:
        return kernel::memory::MemoryRegionKind::Usable;
    case LIMINE_MEMMAP_RESERVED:
        return kernel::memory::MemoryRegionKind::Reserved;
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        return kernel::memory::MemoryRegionKind::AcpiReclaimable;
    case LIMINE_MEMMAP_ACPI_NVS:
        return kernel::memory::MemoryRegionKind::AcpiNvs;
    case LIMINE_MEMMAP_BAD_MEMORY:
        return kernel::memory::MemoryRegionKind::BadMemory;
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
        return kernel::memory::MemoryRegionKind::BootloaderReclaimable;
    case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
        return kernel::memory::MemoryRegionKind::KernelAndModules;
    case LIMINE_MEMMAP_FRAMEBUFFER:
        return kernel::memory::MemoryRegionKind::Framebuffer;
    case LIMINE_MEMMAP_RESERVED_MAPPED:
        return kernel::memory::MemoryRegionKind::ReservedMapped;
    default:
        return kernel::memory::MemoryRegionKind::Unknown;
    }
}

class BootMemoryState
{
public:
    bool init(const limine_memmap_response * response)
    {
        reset();
        if (response == nullptr || response->entries == nullptr)
        {
            return false;
        }

        stats_.initialized = true;
        for (uint64_t index = 0; index < response->entry_count; ++index)
        {
            const limine_memmap_entry * entry = response->entries[index];
            if (entry == nullptr)
            {
                continue;
            }

            append(*entry);
        }

        allocator_.reset(memory_map());
        refresh_stats();
        return true;
    }

    kernel::memory::Stats stats()
    {
        if (stats_.initialized)
        {
            refresh_stats();
        }

        return stats_;
    }

    kernel::memory::MemoryMapView memory_map() const { return {regions_, region_count_}; }

    bool allocate_frame(kernel::memory::PhysicalFrame & frame)
    {
        if (!stats_.initialized)
        {
            frame = {};
            return false;
        }

        const bool allocated = allocator_.allocate(frame);
        refresh_stats();
        return allocated;
    }

private:
    void reset()
    {
        region_count_ = 0;
        allocator_.reset({});
        stats_ = {};
    }

    void append(const limine_memmap_entry & entry)
    {
        if (region_count_ >= kernel::memory::kMaxBootMemoryRegions)
        {
            stats_.truncated = true;
            return;
        }

        regions_[region_count_++] = {
            entry.base,
            entry.length,
            memory_region_kind_from_limine(entry.type),
        };
    }

    void refresh_stats()
    {
        stats_.map = memory_map().stats();
        stats_.frames = allocator_.stats();
    }

    kernel::memory::MemoryRegion regions_[kernel::memory::kMaxBootMemoryRegions] = {};
    size_t region_count_ = 0;
    kernel::memory::EarlyFrameAllocator allocator_;
    kernel::memory::Stats stats_;
};

BootMemoryState g_memory;

} // namespace

namespace kernel::memory
{

bool init() { return g_memory.init(kernel::boot::memory_map()); }

Stats stats() { return g_memory.stats(); }

MemoryMapView memory_map() { return g_memory.memory_map(); }

bool allocate_frame(PhysicalFrame & frame) { return g_memory.allocate_frame(frame); }

} // namespace kernel::memory
