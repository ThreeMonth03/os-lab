#include "kernel/memory/physical_frame_allocator.hpp"

namespace {

uint64_t region_end(const kernel::memory::MemoryRegion& region) {
    const uint64_t end = region.base + region.length;
    if (end < region.base) {
        return UINT64_MAX;
    }

    return end;
}

uint64_t region_start(const kernel::memory::MemoryRegion& region) {
    uint64_t start = kernel::memory::align_up_to_frame(region.base);
    if (start == 0 && region.base != 0) {
        return 0;
    }

    if (start < kernel::memory::kFrameSize) {
        start = kernel::memory::kFrameSize;
    }

    return start;
}

} // namespace

namespace kernel::memory {

uint64_t align_up_to_frame(uint64_t value) {
    const uint64_t mask = kFrameSize - 1;
    if ((value & mask) == 0) {
        return value;
    }

    if (value > UINT64_MAX - mask) {
        return 0;
    }

    return (value + mask) & ~mask;
}

uint64_t align_down_to_frame(uint64_t value) { return value & ~(kFrameSize - 1); }

uint64_t usable_frame_count(const MemoryRegion& region) {
    if (!is_allocatable(region.kind)) {
        return 0;
    }

    const uint64_t start = region_start(region);
    const uint64_t end = align_down_to_frame(region_end(region));
    if (start == 0 || end <= start) {
        return 0;
    }

    return (end - start) / kFrameSize;
}

void EarlyFrameAllocator::reset(MemoryMapView map) {
    regions_ = map.regions();
    region_index_ = 0;
    next_frame_ = 0;
    total_frames_ = 0;
    allocated_frames_ = 0;

    for (const MemoryRegion& region : regions_) {
        total_frames_ += usable_frame_count(region);
    }
}

bool EarlyFrameAllocator::allocate(PhysicalFrame& frame) {
    while (region_index_ < regions_.size()) {
        const MemoryRegion& region = regions_[region_index_];
        if (!is_allocatable(region.kind)) {
            ++region_index_;
            next_frame_ = 0;
            continue;
        }

        const uint64_t start = region_start(region);
        const uint64_t end = align_down_to_frame(region_end(region));
        if (start == 0 || end <= start) {
            ++region_index_;
            next_frame_ = 0;
            continue;
        }

        if (next_frame_ == 0 || next_frame_ < start) {
            next_frame_ = start;
        }

        if (next_frame_ <= end - kFrameSize) {
            frame = PhysicalFrame::from_address(next_frame_);
            next_frame_ += kFrameSize;
            ++allocated_frames_;
            return true;
        }

        ++region_index_;
        next_frame_ = 0;
    }

    frame = {};
    return false;
}

FrameAllocatorStats EarlyFrameAllocator::stats() const {
    return {total_frames_, allocated_frames_, total_frames_ - allocated_frames_};
}

} // namespace kernel::memory
