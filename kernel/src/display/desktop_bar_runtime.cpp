#include "desktop_bar_runtime.hpp"

#include "kernel/display/compositor.hpp"

namespace
{

struct DesktopBarState
{
    kernel::display::GuiSurface bar;
    kernel::display::desktop_bar::Palette palette;
    kernel::display::desktop_bar::Config config;
    kernel::display::desktop_bar::TerminalItemState terminal_item;
    kernel::display::desktop_bar::WindowItemState dummy_item;
    bool initialized = false;
};

DesktopBarState g_state;

kernel::display::PixelSample sample_bar_pixel(uint64_t x, uint64_t y)
{
    if (!g_state.initialized)
    {
        return kernel::display::transparent_pixel();
    }

    return kernel::display::desktop_bar::sample_pixel(g_state.bar,
                                                      g_state.palette,
                                                      g_state.config,
                                                      g_state.terminal_item,
                                                      g_state.dummy_item,
                                                      x,
                                                      y);
}

} // namespace

namespace kernel::display::desktop_bar
{

bool init(const GuiSurface & bar, Palette palette, Config config)
{
    g_state = {};
    if (!bar.valid())
    {
        return false;
    }

    g_state.bar = bar;
    g_state.palette = palette;
    g_state.config = config;
    g_state.terminal_item = {};
    g_state.dummy_item = {};
    if (!compositor::register_layer_pixel_callback(LayerKind::GuiSurface, sample_bar_pixel))
    {
        g_state = {};
        return false;
    }

    g_state.initialized = true;
    return true;
}

void sync_terminal_item_state(TerminalItemState terminal)
{
    if (!g_state.initialized)
    {
        return;
    }

    g_state.terminal_item = terminal;
}

void sync_dummy_debug_item_state(WindowItemState dummy)
{
    if (!g_state.initialized)
    {
        return;
    }

    g_state.dummy_item = dummy;
}

HitTestResult hit_test(uint64_t x, uint64_t y)
{
    if (!g_state.initialized)
    {
        return {};
    }

    return hit_test(g_state.bar, g_state.config, g_state.terminal_item, g_state.dummy_item, x, y);
}

bool action_enabled(DesktopShellAction action)
{
    if (!g_state.initialized)
    {
        return false;
    }

    const ItemList items =
        item_list_for(g_state.bar, g_state.config, g_state.terminal_item, g_state.dummy_item);
    for (size_t index = 0; index < items.count; ++index)
    {
        const Item & item = items.items[index];
        if (item.valid() && item.enabled && item.action == action)
        {
            return true;
        }
    }
    return false;
}

} // namespace kernel::display::desktop_bar
