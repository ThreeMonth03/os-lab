#include "kernel/display/desktop_bar.hpp"

#ifndef OS_LAB_GUI_PANEL_VISIBLE
#define OS_LAB_GUI_PANEL_VISIBLE 0
#endif

#ifndef OS_LAB_DESKTOP_BAR_DEBUG_ACTIONS
#define OS_LAB_DESKTOP_BAR_DEBUG_ACTIONS 0
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

bool on_rect_border(kernel::display::Rect rect, uint64_t x, uint64_t y)
{
    return contains(rect, x, y) &&
           (x == rect.x || y == rect.y || x + 1 == rect.x + rect.width ||
            y + 1 == rect.y + rect.height);
}

uint64_t item_margin_for(kernel::display::Rect bar_bounds)
{
    return min_u64(kernel::display::desktop_bar::kItemMargin,
                   bar_bounds.height / 4);
}

bool terminal_icon_pixel(kernel::display::Rect button_bounds, uint64_t x, uint64_t y)
{
    constexpr uint64_t kIconWidth = 14;
    constexpr uint64_t kIconHeight = 10;
    constexpr uint64_t kIconLeftInset = 8;
    if (button_bounds.width < kIconLeftInset + kIconWidth + 2 ||
        button_bounds.height < kIconHeight + 2)
    {
        return false;
    }

    const uint64_t icon_x = button_bounds.x + kIconLeftInset;
    const uint64_t icon_y = button_bounds.y + ((button_bounds.height - kIconHeight) / 2);
    const kernel::display::Rect icon{icon_x, icon_y, kIconWidth, kIconHeight};
    if (!contains(icon, x, y))
    {
        return false;
    }

    const uint64_t local_x = x - icon.x;
    const uint64_t local_y = y - icon.y;
    const bool monitor_outline = local_x == 0 || local_y == 0 ||
                                 local_x + 1 == kIconWidth || local_y + 2 == kIconHeight;
    const bool prompt_mark = local_x >= 3 && local_x <= 5 &&
                             (local_y == 3 || local_y == 4 || local_y == 5) &&
                             local_x == local_y;
    const bool prompt_tail = local_x == 6 && local_y == 4;
    const bool baseline = local_x >= 8 && local_x <= 11 && local_y == 6;
    return monitor_outline || prompt_mark || prompt_tail || baseline;
}

} // namespace

namespace kernel::display::desktop_bar
{

Config default_config()
{
    Config config;
    config.visible = OS_LAB_GUI_PANEL_VISIBLE != 0;
    config.debug_actions = OS_LAB_DESKTOP_BAR_DEBUG_ACTIONS != 0;
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

bool Item::valid() const
{
    return visible && kind != ItemKind::None && action != DesktopShellAction::None &&
           !bounds.empty();
}

bool HitTestResult::hit_item() const
{
    return region == HitRegion::Item && item_kind != ItemKind::None &&
           action != DesktopShellAction::None;
}

Item terminal_item_for(const GuiSurface & surface, Config config, TerminalItemState terminal)
{
    if (!surface.valid() || !surface.visible || !config.debug_actions)
    {
        return {};
    }

    const uint64_t margin = item_margin_for(surface.bounds);
    if (surface.bounds.width <= margin * 2 || surface.bounds.height <= margin * 2)
    {
        return {};
    }

    const uint64_t available_width = surface.bounds.width - (margin * 2);
    const uint64_t button_width = min_u64(kDefaultItemWidth, available_width);
    const uint64_t button_height = surface.bounds.height - (margin * 2);
    if (button_width < kItemMinWidth || button_height < kItemMinHeight)
    {
        return {};
    }

    const bool selected = terminal.app_visible && terminal.app_focused && terminal.app_active &&
                          !terminal.app_closed;

    return {
        {surface.bounds.x + margin,
         surface.bounds.y + ((surface.bounds.height - button_height) / 2),
         button_width,
         button_height},
        ItemKind::Terminal,
        DesktopShellAction::TerminalShowFocus,
        true,
        !terminal.app_closed && !selected,
        terminal.app_visible && terminal.app_active && !terminal.app_closed,
        terminal.app_focused && terminal.app_visible && !terminal.app_closed,
    };
}

HitTestResult hit_test(const GuiSurface & surface,
                       Config config,
                       TerminalItemState terminal,
                       uint64_t x,
                       uint64_t y)
{
    if (!surface.valid() || !surface.visible || !contains(surface.bounds, x, y))
    {
        return {};
    }

    const Item terminal_item = terminal_item_for(surface, config, terminal);
    if (terminal_item.valid() && contains(terminal_item.bounds, x, y))
    {
        return {
            HitRegion::Item,
            terminal_item.kind,
            terminal_item.action,
            terminal_item.enabled,
        };
    }

    return {
        HitRegion::Background,
        ItemKind::None,
        DesktopShellAction::None,
        false,
    };
}

PixelSample sample_pixel(const GuiSurface & surface,
                         Palette palette,
                         Config config,
                         TerminalItemState terminal,
                         uint64_t x,
                         uint64_t y)
{
    if (!surface.valid() || !surface.visible || !contains(surface.bounds, x, y))
    {
        return transparent_pixel();
    }

    const Item terminal_item = terminal_item_for(surface, config, terminal);
    if (terminal_item.valid() && contains(terminal_item.bounds, x, y))
    {
        if (on_rect_border(terminal_item.bounds, x, y))
        {
            return opaque_pixel(palette.button_border);
        }
        if (terminal_icon_pixel(terminal_item.bounds, x, y))
        {
            return opaque_pixel(palette.button_icon);
        }

        return opaque_pixel(terminal_item.enabled ? palette.button_background
                                                  : palette.button_disabled_background);
    }

    if (y < surface.bounds.y + kTopBorderHeight)
    {
        return opaque_pixel(palette.border);
    }
    return opaque_pixel(palette.background);
}

} // namespace kernel::display::desktop_bar
