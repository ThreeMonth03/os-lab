#include "gui_panel_runtime.hpp"

#include "kernel/display/compositor.hpp"
#include "kernel/display/gui_panel_renderer.hpp"

namespace
{

struct PanelState
{
    kernel::display::GuiSurface panel;
    kernel::display::gui_panel::Palette palette;
    kernel::display::gui_panel::Config config;
    bool initialized = false;
};

PanelState g_state;

bool panel_ready()
{
    return g_state.initialized;
}

kernel::display::PixelSample sample_panel_pixel(uint64_t x, uint64_t y)
{
    if (!panel_ready())
    {
        return kernel::display::transparent_pixel();
    }

    return kernel::display::gui_panel::sample_pixel(g_state.panel, g_state.palette, x, y);
}

} // namespace

namespace kernel::display::gui_panel
{

bool init(const GuiSurface & panel, Color border, Color background, Color foreground, Config config)
{
    g_state = {};
    if (!panel.valid())
    {
        return false;
    }

    g_state.panel = panel;
    g_state.palette = {border, background, foreground};
    g_state.config = config;
    if (!compositor::register_layer_pixel_callback(LayerKind::GuiSurface, sample_panel_pixel))
    {
        g_state = {};
        return false;
    }

    g_state.initialized = true;
    return true;
}

} // namespace kernel::display::gui_panel
