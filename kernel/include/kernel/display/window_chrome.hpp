#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

enum class WindowChromeHitRegion
{
    None,
    Border,
    TitleBar,
    Content,
    CloseButton,
    ResizeLeft,
    ResizeRight,
    ResizeTop,
    ResizeBottom,
    ResizeTopLeft,
    ResizeTopRight,
    ResizeBottomLeft,
    ResizeBottomRight,
};

struct WindowFrameConfig
{
    bool visible = false;
    uint64_t border_thickness = 1;
    uint64_t title_bar_height = 20;
    uint64_t close_button_size = 10;
    uint64_t chrome_margin = 4;
    uint64_t resize_handle_size = 12;
};

constexpr WindowFrameConfig terminal_window_frame_config(bool visible)
{
    WindowFrameConfig config;
    config.visible = visible;
    return config;
}

struct WindowFrameMetrics
{
    Rect outer_bounds;
    Rect title_bar_bounds;
    Rect client_bounds;
    Rect close_button_bounds;
    Rect resize_handle_bounds;
    uint64_t border_thickness = 0;
    bool visible = false;

    bool valid() const { return !outer_bounds.empty() && !client_bounds.empty(); }
};

class WindowChrome
{
public:
    static WindowFrameMetrics metrics_for(Rect outer_bounds, WindowFrameConfig config);
    static WindowChromeHitRegion hit_test(WindowFrameMetrics metrics, uint64_t x, uint64_t y);
};

} // namespace kernel::display
