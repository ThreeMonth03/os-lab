#pragma once

#include <stddef.h>

#include "kernel/memory/memory_map.hpp"
#include "kernel/memory/physical_frame_allocator.hpp"

namespace kernel::memory {

constexpr size_t kMaxBootMemoryRegions = 64;

struct Stats {
    bool initialized = false;
    bool truncated = false;
    MemoryMapStats map;
    FrameAllocatorStats frames;
};

[[nodiscard]] bool init();
[[nodiscard]] Stats stats();
[[nodiscard]] MemoryMapView memory_map();
[[nodiscard]] bool allocate_frame(PhysicalFrame& frame);

} // namespace kernel::memory
