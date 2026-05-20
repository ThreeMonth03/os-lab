#pragma once

#include <stdint.h>

#include "kernel/display/backing_surface.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/window_chrome.hpp"
#include "kernel/display/window_repaint_planner.hpp"

namespace kernel::display
{

class WindowPreviewShape
{
public:
    WindowPreviewShape() = default;
    WindowPreviewShape(Rect desktop_bounds, WindowFrameConfig frame_config);

    [[nodiscard]] WindowRepaintRegionList damage_for(Rect outer_bounds) const;
    [[nodiscard]] PixelSample sample(Rect outer_bounds, Color color, uint64_t x, uint64_t y) const;

private:
    [[nodiscard]] static bool contains(Rect rect, uint64_t x, uint64_t y);
    [[nodiscard]] static bool should_paint(WindowFrameMetrics metrics, uint64_t x, uint64_t y);
    static void append_paint_regions(WindowRepaintRegionList & regions,
                                     WindowFrameMetrics metrics);

    Rect desktop_bounds_;
    WindowFrameConfig frame_config_;
};

} // namespace kernel::display
