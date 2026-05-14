#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/span.hpp"

namespace kernel::memory {

enum class MemoryRegionKind {
    Usable,
    Reserved,
    AcpiReclaimable,
    AcpiNvs,
    BadMemory,
    BootloaderReclaimable,
    KernelAndModules,
    Framebuffer,
    ReservedMapped,
    Unknown,
};

struct MemoryRegion {
    uint64_t base = 0;
    uint64_t length = 0;
    MemoryRegionKind kind = MemoryRegionKind::Unknown;
};

struct MemoryMapStats {
    size_t region_count = 0;
    uint64_t total_bytes = 0;
    uint64_t usable_bytes = 0;
    uint64_t bootloader_reclaimable_bytes = 0;
    uint64_t framebuffer_bytes = 0;
    uint64_t reserved_bytes = 0;
};

[[nodiscard]] bool is_allocatable(MemoryRegionKind kind);
[[nodiscard]] bool is_reclaimable(MemoryRegionKind kind);
[[nodiscard]] bool is_reserved(MemoryRegionKind kind);

class MemoryMapView {
  public:
    MemoryMapView() = default;
    MemoryMapView(const MemoryRegion* regions, size_t count) : regions_(regions, count) {}
    explicit MemoryMapView(Span<const MemoryRegion> regions) : regions_(regions) {}

    [[nodiscard]] bool empty() const { return regions_.empty(); }
    [[nodiscard]] size_t size() const { return regions_.size(); }
    [[nodiscard]] Span<const MemoryRegion> regions() const { return regions_; }
    [[nodiscard]] const MemoryRegion& operator[](size_t index) const { return regions_[index]; }
    [[nodiscard]] MemoryMapStats stats() const;

  private:
    Span<const MemoryRegion> regions_;
};

} // namespace kernel::memory
