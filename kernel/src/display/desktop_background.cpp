#include "kernel/display/desktop_background.hpp"

namespace kernel::display::desktop_background
{

Rect bounds_for(uint64_t surface_width, uint64_t surface_height)
{
    return {0, 0, surface_width, surface_height};
}

Rect repaint_region(Rect bounds, Rect dirty_rect)
{
    return intersect_rect(bounds, dirty_rect);
}

void paint_solid(Surface & surface, Rect bounds, SolidColorSource source, Rect dirty_rect)
{
    if (!surface.ready())
    {
        return;
    }

    const Rect region = repaint_region(bounds, dirty_rect);
    if (!region.empty())
    {
        surface.fill_rect(region, source.color);
    }
}

} // namespace kernel::display::desktop_background
