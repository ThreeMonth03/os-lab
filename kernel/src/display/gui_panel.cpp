#include "kernel/display/gui_panel.hpp"

namespace
{

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

} // namespace

namespace kernel::display::gui_panel
{

Rect bounds_for(uint64_t surface_width, uint64_t surface_height, Config config)
{
    if (surface_width <= config.margin * 2 || surface_height <= config.margin * 2 ||
        config.width == 0 || config.height == 0)
    {
        return {};
    }

    const uint64_t available_width = surface_width - (config.margin * 2);
    const uint64_t available_height = surface_height - (config.margin * 2);
    const uint64_t width = min_u64(config.width, available_width);
    const uint64_t height = min_u64(config.height, available_height);
    if (width == 0 || height == 0)
    {
        return {};
    }

    return {config.margin, config.margin, width, height};
}

Rect content_bounds(Rect panel_bounds, uint64_t padding)
{
    if (panel_bounds.empty() || panel_bounds.width <= padding * 2 ||
        panel_bounds.height <= padding * 2)
    {
        return {};
    }

    return {
        panel_bounds.x + padding,
        panel_bounds.y + padding,
        panel_bounds.width - (padding * 2),
        panel_bounds.height - (padding * 2),
    };
}

GuiSurface make_surface(uint64_t surface_width, uint64_t surface_height, GuiSurfaceId id, Config config)
{
    return make_gui_surface(id, bounds_for(surface_width, surface_height, config), config.visible, false);
}

bool should_redraw(const GuiSurface & surface)
{
    return surface.valid() && surface.visible;
}

} // namespace kernel::display::gui_panel
