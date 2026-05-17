#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

inline constexpr size_t kMaxPresentOperations = 64;

enum class PresentOperationKind
{
    None,
    Rect,
    Scroll,
};

enum class PresentOperationAppendResult
{
    Ignored,
    Queued,
    Merged,
    FullscreenFallback,
};

struct PresentOperation
{
    PresentOperationKind kind = PresentOperationKind::None;
    Rect rect;
    uint64_t distance = 0;

    bool rect_present() const { return kind == PresentOperationKind::Rect && !rect.empty(); }
    bool scroll_present() const
    {
        return kind == PresentOperationKind::Scroll && !rect.empty() && distance > 0;
    }
};

struct PresentOperationStats
{
    uint64_t operation_count = 0;
    uint64_t rect_count = 0;
    uint64_t scroll_count = 0;
    uint64_t total_presented_pixels = 0;
    uint64_t largest_present_rect_area = 0;
    uint64_t large_present_fallback_count = 0;
};

class PresentOperationList
{
public:
    PresentOperationList() = default;
    explicit PresentOperationList(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void clear();
    PresentOperationAppendResult append_rect(Rect rect);
    PresentOperationAppendResult append_scroll(Rect rect, uint64_t distance);

    bool empty() const { return count_ == 0; }
    size_t count() const { return count_; }
    size_t capacity() const { return kMaxPresentOperations; }
    bool full_screen_fallback() const { return full_screen_fallback_; }
    Rect bounds() const { return bounds_; }
    PresentOperationStats stats() const { return stats_; }

    PresentOperation at(size_t index) const;

private:
    PresentOperationAppendResult fallback_to_fullscreen();
    bool can_merge_last_rect(Rect rect) const;
    void rebuild_stats();

    Rect bounds_;
    PresentOperation operations_[kMaxPresentOperations] = {};
    size_t count_ = 0;
    bool full_screen_fallback_ = false;
    PresentOperationStats stats_;
}; // end class PresentOperationList

} // namespace kernel::display
