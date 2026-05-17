#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

inline constexpr size_t kMaxPresentRegions = 32;

enum class PresentRegionAppendResult
{
    Ignored,
    Queued,
    Merged,
    FullscreenFallback,
};

struct PresentRegionStats
{
    uint64_t append_count = 0;
    uint64_t merge_count = 0;
    uint64_t fallback_count = 0;
    uint64_t presented_pixels = 0;
    uint64_t largest_region_area = 0;
};

class PresentRegionList
{
public:
    PresentRegionList() = default;
    explicit PresentRegionList(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void clear();
    PresentRegionAppendResult append(Rect rect);

    bool empty() const { return count_ == 0; }
    size_t count() const { return count_; }
    size_t capacity() const { return kMaxPresentRegions; }
    Rect bounds() const { return bounds_; }
    bool full_screen_fallback() const { return full_screen_fallback_; }
    PresentRegionStats stats() const { return stats_; }

    [[nodiscard]] Rect at(size_t index) const;

private:
    [[nodiscard]] PresentRegionAppendResult fallback_to_fullscreen();
    [[nodiscard]] bool merge_region_at(size_t index);

    Rect bounds_;
    Rect regions_[kMaxPresentRegions] = {};
    size_t count_ = 0;
    bool full_screen_fallback_ = false;
    PresentRegionStats stats_;
}; // end class PresentRegionList

} // namespace kernel::display
