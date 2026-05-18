#include "kernel/display/desktop_bar.hpp"

#ifndef OS_LAB_GUI_PANEL_VISIBLE
#define OS_LAB_GUI_PANEL_VISIBLE 0
#endif

namespace
{

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

bool contains(kernel::display::Rect rect, uint64_t x, uint64_t y)
{
    return !rect.empty() && x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

} // namespace

namespace kernel::display::desktop_bar
{

Config default_config()
{
    Config config;
    config.visible = OS_LAB_GUI_PANEL_VISIBLE != 0;
    return config;
}

Rect bounds_for(Rect desktop_bounds, Config config)
{
    if (desktop_bounds.empty() || config.height == 0)
    {
        return {};
    }

    const uint64_t height = min_u64(config.height, desktop_bounds.height);
    if (height < kMinimumHeight)
    {
        return {};
    }

    return {
        desktop_bounds.x,
        desktop_bounds.y + desktop_bounds.height - height,
        desktop_bounds.width,
        height,
    };
}

Rect work_area_for(Rect desktop_bounds, Rect bar_bounds, bool bar_visible)
{
    if (desktop_bounds.empty())
    {
        return {};
    }
    if (!bar_visible || bar_bounds.empty())
    {
        return desktop_bounds;
    }

    const Rect overlap = intersect_rect(desktop_bounds, bar_bounds);
    if (overlap.empty() ||
        overlap.y + overlap.height < desktop_bounds.y + desktop_bounds.height)
    {
        return desktop_bounds;
    }

    return {
        desktop_bounds.x,
        desktop_bounds.y,
        desktop_bounds.width,
        overlap.y - desktop_bounds.y,
    };
}

Layout layout_for(Rect desktop_bounds, Config config)
{
    const Rect bar_bounds = bounds_for(desktop_bounds, config);
    return {
        desktop_bounds,
        bar_bounds,
        work_area_for(desktop_bounds, bar_bounds, config.visible),
        config.visible && !bar_bounds.empty(),
    };
}

GuiSurface make_surface(Rect desktop_bounds, GuiSurfaceId id, Config config)
{
    const Layout layout = layout_for(desktop_bounds, config);
    return make_gui_surface(id, layout.bar_bounds, layout.bar_visible, false);
}

bool should_redraw(const GuiSurface & surface)
{
    return surface.valid() && surface.visible;
}

PixelSample sample_pixel(const GuiSurface & surface, Palette palette, uint64_t x, uint64_t y)
{
    if (!surface.valid() || !surface.visible || !contains(surface.bounds, x, y))
    {
        return transparent_pixel();
    }

    if (y < surface.bounds.y + kTopBorderHeight)
    {
        return opaque_pixel(palette.border);
    }
    return opaque_pixel(palette.background);
}

} // namespace kernel::display::desktop_bar
