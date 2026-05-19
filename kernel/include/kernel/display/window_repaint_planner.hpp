#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/window_chrome.hpp"
#include "kernel/display/window_session.hpp"
#include "kernel/display/window_stack.hpp"

namespace kernel::display
{

inline constexpr size_t kMaxWindowRepaintRegions = 16;

enum class WindowRepaintAppendResult
{
    Ignored,
    Queued,
    FullscreenFallback,
};

class WindowRepaintRegionList
{
public:
    WindowRepaintRegionList() = default;
    explicit WindowRepaintRegionList(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void clear();
    WindowRepaintAppendResult append(Rect rect);

    bool empty() const { return count_ == 0; }
    size_t count() const { return count_; }
    size_t capacity() const { return kMaxWindowRepaintRegions; }
    bool full_screen_fallback() const { return full_screen_fallback_; }
    uint64_t total_area() const { return total_area_; }
    uint64_t largest_area() const { return largest_area_; }
    Rect bounds() const { return bounds_; }
    Rect at(size_t index) const;

private:
    WindowRepaintAppendResult fallback_to_fullscreen();
    void rebuild_stats();

    Rect bounds_;
    Rect regions_[kMaxWindowRepaintRegions] = {};
    size_t count_ = 0;
    bool full_screen_fallback_ = false;
    uint64_t total_area_ = 0;
    uint64_t largest_area_ = 0;
};

class WindowRepaintPlanner
{
public:
    WindowRepaintPlanner() = default;
    WindowRepaintPlanner(Rect desktop_bounds, WindowFrameConfig frame_config);

    [[nodiscard]] WindowRepaintRegionList move_damage(Rect previous_bounds,
                                                      Rect current_bounds) const;
    [[nodiscard]] WindowRepaintRegionList mutation_damage(
        WindowSessionMutation mutation) const;
    [[nodiscard]] WindowRepaintRegionList visual_state_damage(Rect outer_bounds) const;
    [[nodiscard]] WindowRepaintRegionList stack_transition_damage(
        const WindowStack & previous,
        const WindowStack & current,
        const WindowSessionRegistry & sessions) const;

private:
    void append_chrome_damage(WindowRepaintRegionList & regions, Rect outer_bounds) const;

    Rect desktop_bounds_;
    WindowFrameConfig frame_config_;
};

} // namespace kernel::display
