#include "kernel/memory.hpp"

#include <limine.h>

#include "kernel/limine_support.hpp"

namespace {

kernel::memory::MemoryRegion g_regions[kernel::memory::kMaxBootMemoryRegions];
size_t g_region_count = 0;
kernel::memory::EarlyFrameAllocator g_allocator;
kernel::memory::Stats g_stats;

kernel::memory::MemoryRegionKind memory_region_kind_from_limine(uint64_t type) {
    switch (type) {
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

kernel::memory::MemoryMapView current_memory_map() { return {g_regions, g_region_count}; }

void refresh_stats() {
    g_stats.map = current_memory_map().stats();
    g_stats.frames = g_allocator.stats();
}

} // namespace

namespace kernel::memory {

bool init() {
    g_region_count = 0;
    g_allocator.reset({});
    g_stats = {};

    const limine_memmap_response* response = kernel::boot::memory_map();
    if (response == nullptr || response->entries == nullptr) {
        return false;
    }

    g_stats.initialized = true;
    for (uint64_t index = 0; index < response->entry_count; ++index) {
        const limine_memmap_entry* entry = response->entries[index];
        if (entry == nullptr) {
            continue;
        }

        const MemoryRegion region{
            entry->base,
            entry->length,
            memory_region_kind_from_limine(entry->type),
        };
        if (g_region_count < kMaxBootMemoryRegions) {
            g_regions[g_region_count++] = region;
        } else {
            g_stats.truncated = true;
        }
    }

    g_allocator.reset(current_memory_map());
    refresh_stats();
    return true;
}

Stats stats() {
    if (g_stats.initialized) {
        refresh_stats();
    }

    return g_stats;
}

MemoryMapView memory_map() { return current_memory_map(); }

bool allocate_frame(PhysicalFrame& frame) {
    if (!g_stats.initialized) {
        frame = {};
        return false;
    }

    const bool allocated = g_allocator.allocate(frame);
    refresh_stats();
    return allocated;
}

} // namespace kernel::memory
