#include "kernel/display/desktop_background.hpp"

namespace kernel::display::desktop_background
{

BackgroundSource solid_background(Color color)
{
    return {
        {color},
        {},
    };
}

BackgroundSource wallpaper_background(Color color, ImageView image, ColorLayout color_layout)
{
    return {
        {color},
        {image, color_layout, 0, 0},
    };
}

Rect bounds_for(uint64_t surface_width, uint64_t surface_height)
{
    return {0, 0, surface_width, surface_height};
}

Rect repaint_region(Rect bounds, Rect dirty_rect)
{
    return intersect_rect(bounds, dirty_rect);
}

void paint(Surface & surface, Rect bounds, BackgroundSource source, Rect dirty_rect)
{
    if (!surface.ready())
    {
        return;
    }

    const Rect region = repaint_region(bounds, dirty_rect);
    if (!region.empty())
    {
        surface.fill_rect(region, source.solid.color);
    }

    if (source.has_wallpaper())
    {
        blit_image(surface,
                   source.wallpaper.image,
                   bounds.x + source.wallpaper.x,
                   bounds.y + source.wallpaper.y,
                   region,
                   source.wallpaper.color_layout);
    }
}

void paint_solid(Surface & surface, Rect bounds, SolidColorSource source, Rect dirty_rect)
{
    paint(surface, bounds, solid_background(source.color), dirty_rect);
}

} // namespace kernel::display::desktop_background
