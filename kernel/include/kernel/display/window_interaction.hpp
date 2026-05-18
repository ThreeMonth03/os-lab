#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/window_chrome.hpp"

namespace kernel::display
{

struct WindowResizeConstraints
{
    Rect work_area;
    uint64_t minimum_client_width = 1;
    uint64_t minimum_client_height = 1;
    WindowFrameConfig frame_config;
};

class WindowResizeDrag
{
public:
    WindowResizeDrag() = default;

    static WindowResizeDrag begin(Rect start_bounds,
                                  uint64_t anchor_x,
                                  uint64_t anchor_y,
                                  WindowResizeConstraints constraints);

    bool valid() const;
    Rect bounds_for(uint64_t pointer_x, uint64_t pointer_y) const;

private:
    WindowResizeDrag(Rect start_bounds,
                     uint64_t anchor_x,
                     uint64_t anchor_y,
                     WindowResizeConstraints constraints);

    Rect start_bounds_;
    uint64_t anchor_x_ = 0;
    uint64_t anchor_y_ = 0;
    WindowResizeConstraints constraints_;
};

} // namespace kernel::display
