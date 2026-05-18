#include "desktop_bar_runtime.hpp"

#include "kernel/display/compositor.hpp"

namespace
{

struct DesktopBarState
{
    kernel::display::GuiSurface bar;
    kernel::display::desktop_bar::Palette palette;
    kernel::display::desktop_bar::Config config;
    bool initialized = false;
};

DesktopBarState g_state;

kernel::display::PixelSample sample_bar_pixel(uint64_t x, uint64_t y)
{
    if (!g_state.initialized)
    {
        return kernel::display::transparent_pixel();
    }

    return kernel::display::desktop_bar::sample_pixel(g_state.bar, g_state.palette, x, y);
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
    if (!compositor::register_layer_pixel_callback(LayerKind::GuiSurface, sample_bar_pixel))
    {
        g_state = {};
        return false;
    }

    g_state.initialized = true;
    return true;
}

} // namespace kernel::display::desktop_bar
