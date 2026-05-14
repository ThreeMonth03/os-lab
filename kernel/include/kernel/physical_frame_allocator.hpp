#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/memory_map.hpp"
#include "kernel/span.hpp"

namespace kernel::memory {

constexpr uint64_t kFrameSize = 4096;

struct PhysicalFrame {
    uint64_t address = 0;
};

struct FrameAllocatorStats {
    uint64_t total_frames = 0;
    uint64_t allocated_frames = 0;
    uint64_t remaining_frames = 0;
};

[[nodiscard]] uint64_t align_up_to_frame(uint64_t value);
[[nodiscard]] uint64_t align_down_to_frame(uint64_t value);
[[nodiscard]] uint64_t usable_frame_count(const MemoryRegion& region);

class EarlyFrameAllocator {
  public:
    EarlyFrameAllocator() = default;
    explicit EarlyFrameAllocator(MemoryMapView map) { reset(map); }

    void reset(MemoryMapView map);
    [[nodiscard]] bool allocate(PhysicalFrame& frame);
    [[nodiscard]] FrameAllocatorStats stats() const;

  private:
    Span<const MemoryRegion> regions_;
    size_t region_index_ = 0;
    uint64_t next_frame_ = 0;
    uint64_t total_frames_ = 0;
    uint64_t allocated_frames_ = 0;
};

} // namespace kernel::memory
