#include "kernel/display/window_preview.hpp"

namespace kernel::display
{

namespace
{

uint64_t rect_right(Rect rect)
{
    return rect.x + rect.width;
}

uint64_t rect_bottom(Rect rect)
{
    return rect.y + rect.height;
}

} // namespace

WindowPreviewShape::WindowPreviewShape(Rect desktop_bounds, WindowFrameConfig frame_config)
    : desktop_bounds_(desktop_bounds)
    , frame_config_(frame_config)
{
}

bool WindowPreviewShape::contains(Rect rect, uint64_t x, uint64_t y)
{
    return !rect.empty() && x >= rect.x && y >= rect.y && x < rect_right(rect) &&
           y < rect_bottom(rect);
}

void WindowPreviewShape::append_paint_regions(WindowRepaintRegionList & regions,
                                              WindowFrameMetrics metrics)
{
    if (!metrics.valid() || !metrics.visible)
    {
        return;
    }

    regions.append(metrics.title_bar_bounds);
    const uint64_t border = metrics.border_thickness;
    if (border == 0 || metrics.outer_bounds.height <= metrics.title_bar_bounds.height)
    {
        return;
    }

    const uint64_t chrome_top = rect_bottom(metrics.title_bar_bounds);
    const uint64_t chrome_height = rect_bottom(metrics.outer_bounds) - chrome_top;
    regions.append({metrics.outer_bounds.x, chrome_top, border, chrome_height});
    regions.append({rect_right(metrics.outer_bounds) - border, chrome_top, border, chrome_height});
    regions.append({metrics.outer_bounds.x,
                    rect_bottom(metrics.outer_bounds) - border,
                    metrics.outer_bounds.width,
                    border});
}

bool WindowPreviewShape::should_paint(WindowFrameMetrics metrics, uint64_t x, uint64_t y)
{
    if (!metrics.valid() || !metrics.visible)
    {
        return false;
    }

    if (contains(metrics.title_bar_bounds, x, y))
    {
        return true;
    }

    const uint64_t border = metrics.border_thickness;
    if (border == 0 || !contains(metrics.outer_bounds, x, y))
    {
        return false;
    }

    const uint64_t chrome_top = rect_bottom(metrics.title_bar_bounds);
    const uint64_t right = rect_right(metrics.outer_bounds);
    const uint64_t bottom = rect_bottom(metrics.outer_bounds);
    return y >= chrome_top &&
           (x < metrics.outer_bounds.x + border || x >= right - border ||
            y >= bottom - border);
}

WindowRepaintRegionList WindowPreviewShape::damage_for(Rect outer_bounds) const
{
    WindowRepaintRegionList regions(desktop_bounds_);
    append_paint_regions(regions, WindowChrome::metrics_for(outer_bounds, frame_config_));
    return regions;
}

PixelSample WindowPreviewShape::sample(Rect outer_bounds,
                                       Color color,
                                       uint64_t x,
                                       uint64_t y) const
{
    if (!contains(outer_bounds, x, y))
    {
        return transparent_pixel();
    }

    const WindowFrameMetrics metrics = WindowChrome::metrics_for(outer_bounds, frame_config_);
    if (!should_paint(metrics, x, y))
    {
        return transparent_pixel();
    }

    return ((x + y) % 2) == 0 ? opaque_pixel(color) : transparent_pixel();
}

} // namespace kernel::display
