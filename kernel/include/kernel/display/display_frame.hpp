#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/present_region_list.hpp"

namespace kernel::display
{

struct DisplayFrameStats
{
    uint64_t frame_flush_count = 0;
    uint64_t present_rect_count = 0;
    uint64_t total_presented_pixels = 0;
    uint64_t largest_present_rect_area = 0;
    uint64_t large_present_fallback_count = 0;
};

struct DisplayFrameFlush
{
    bool outermost_frame_ended = false;
    PresentRegionList present_regions;
};

struct DisplayFrameSubmit
{
    bool immediate = false;
    PresentRegionList present_regions;
};

class DisplayFrame
{
public:
    DisplayFrame() = default;
    explicit DisplayFrame(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void begin();
    [[nodiscard]] DisplayFrameFlush end();
    [[nodiscard]] DisplayFrameSubmit submit(Rect present_rect);
    [[nodiscard]] DisplayFrameSubmit submit(const PresentRegionList & present_regions);

    bool in_frame() const { return depth_ > 0; }
    uint32_t depth() const { return depth_; }
    Rect bounds() const { return bounds_; }
    PresentRegionList pending() const { return present_regions_; }
    DisplayFrameStats stats() const { return stats_; }

private:
    void record_stats(const PresentRegionList & present_regions, bool frame_flush);

    uint32_t depth_ = 0;
    Rect bounds_;
    PresentRegionList present_regions_;
    DisplayFrameStats stats_;
};

} // namespace kernel::display
