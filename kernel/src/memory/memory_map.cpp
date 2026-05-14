#include "kernel/memory/memory_map.hpp"

namespace {

uint64_t add_saturating(uint64_t left, uint64_t right) {
    const uint64_t value = left + right;
    if (value < left) {
        return UINT64_MAX;
    }

    return value;
}

} // namespace

namespace kernel::memory {

bool is_allocatable(MemoryRegionKind kind) { return kind == MemoryRegionKind::Usable; }

bool is_reclaimable(MemoryRegionKind kind) {
    return kind == MemoryRegionKind::Usable || kind == MemoryRegionKind::BootloaderReclaimable ||
           kind == MemoryRegionKind::AcpiReclaimable;
}

bool is_reserved(MemoryRegionKind kind) {
    return kind == MemoryRegionKind::Reserved || kind == MemoryRegionKind::AcpiNvs ||
           kind == MemoryRegionKind::BadMemory || kind == MemoryRegionKind::KernelAndModules ||
           kind == MemoryRegionKind::Framebuffer || kind == MemoryRegionKind::ReservedMapped ||
           kind == MemoryRegionKind::Unknown;
}

MemoryMapStats MemoryMapView::stats() const {
    MemoryMapStats result;
    result.region_count = regions_.size();

    for (const MemoryRegion& region : regions_) {
        result.total_bytes = add_saturating(result.total_bytes, region.length);

        switch (region.kind) {
        case MemoryRegionKind::Usable:
            result.usable_bytes = add_saturating(result.usable_bytes, region.length);
            break;
        case MemoryRegionKind::BootloaderReclaimable:
            result.bootloader_reclaimable_bytes =
                add_saturating(result.bootloader_reclaimable_bytes, region.length);
            break;
        case MemoryRegionKind::Framebuffer:
            result.framebuffer_bytes = add_saturating(result.framebuffer_bytes, region.length);
            break;
        case MemoryRegionKind::Reserved:
        case MemoryRegionKind::AcpiReclaimable:
        case MemoryRegionKind::AcpiNvs:
        case MemoryRegionKind::BadMemory:
        case MemoryRegionKind::KernelAndModules:
        case MemoryRegionKind::ReservedMapped:
        case MemoryRegionKind::Unknown:
            result.reserved_bytes = add_saturating(result.reserved_bytes, region.length);
            break;
        }
    }

    return result;
}

} // namespace kernel::memory
