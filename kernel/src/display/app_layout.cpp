#include "kernel/display/app_layout.hpp"

namespace
{

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

bool has_cell_capacity(kernel::display::Rect bounds, uint64_t cell_width, uint64_t cell_height)
{
    return cell_width > 0 && cell_height > 0 && bounds.width >= cell_width &&
           bounds.height >= cell_height;
}

kernel::display::Rect safe_hidden_panel_bounds(kernel::display::Rect desktop_bounds,
                                               uint64_t top_safe_inset,
                                               uint64_t cell_width,
                                               uint64_t cell_height)
{
    if (!has_cell_capacity(desktop_bounds, cell_width, cell_height))
    {
        return {};
    }

    uint64_t safe_inset = top_safe_inset;
    if (desktop_bounds.height <= cell_height)
    {
        safe_inset = 0;
    }
    else if (safe_inset > desktop_bounds.height - cell_height)
    {
        safe_inset = desktop_bounds.height - cell_height;
    }

    return {
        desktop_bounds.x,
        desktop_bounds.y + safe_inset,
        desktop_bounds.width,
        desktop_bounds.height - safe_inset,
    };
}

} // namespace

namespace kernel::display
{

Rect TerminalAppLayout::bounds_for(TerminalAppLayoutConfig config)
{
    if (!has_cell_capacity(config.desktop_bounds, config.cell_width, config.cell_height))
    {
        return {};
    }

    const Rect hidden_bounds = safe_hidden_panel_bounds(config.desktop_bounds,
                                                        config.hidden_panel_top_safe_inset,
                                                        config.cell_width,
                                                        config.cell_height);
    if (!config.panel_visible || config.panel_bounds.empty())
    {
        return hidden_bounds;
    }

    const uint64_t margin = config.panel_margin;
    const uint64_t left = config.desktop_bounds.x + margin;
    const uint64_t top = config.panel_bounds.y + config.panel_bounds.height + margin;
    const uint64_t desktop_right = config.desktop_bounds.x + config.desktop_bounds.width;
    const uint64_t desktop_bottom = config.desktop_bounds.y + config.desktop_bounds.height;
    if (left >= desktop_right || top >= desktop_bottom)
    {
        return hidden_bounds;
    }

    const uint64_t right_margin = max_u64(margin, left - config.desktop_bounds.x);
    if (config.desktop_bounds.width <= (left - config.desktop_bounds.x) + right_margin ||
        desktop_bottom <= top + margin)
    {
        return hidden_bounds;
    }

    const Rect panel_adjusted_bounds{
        left,
        top,
        desktop_right - left - right_margin,
        desktop_bottom - top - margin,
    };
    if (!has_cell_capacity(panel_adjusted_bounds, config.cell_width, config.cell_height))
    {
        return hidden_bounds;
    }

    return panel_adjusted_bounds;
}

} // namespace kernel::display
