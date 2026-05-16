#include "gui_panel_runtime.hpp"

#include "kernel/display/compositor.hpp"
#include "kernel/display/gui_panel_renderer.hpp"

namespace
{

struct PanelState
{
    kernel::display::Surface * surface = nullptr;
    kernel::display::GuiSurface panel;
    kernel::display::gui_panel::Palette palette;
    kernel::display::gui_panel::Config config;
    bool initialized = false;
};

PanelState g_state;

bool panel_ready()
{
    return g_state.initialized && g_state.surface != nullptr && g_state.surface->ready();
}

void paint_panel_region(kernel::display::Rect dirty_rect)
{
    if (!panel_ready() || !kernel::display::gui_panel::should_redraw(g_state.panel))
    {
        return;
    }

    kernel::display::gui_panel::paint_region(*g_state.surface,
                                             g_state.panel,
                                             g_state.palette,
                                             dirty_rect);
}

} // namespace

namespace kernel::display::gui_panel
{

bool init(Surface & surface, const GuiSurface & panel, Color border, Color background, Color foreground, Config config)
{
    g_state = {};
    if (!surface.ready() || !panel.valid())
    {
        return false;
    }

    g_state.surface = &surface;
    g_state.panel = panel;
    g_state.palette = {border, background, foreground};
    g_state.config = config;
    if (!compositor::register_layer_repaint_callback(LayerKind::GuiSurface, paint_panel_region))
    {
        g_state = {};
        return false;
    }

    g_state.initialized = true;
    return true;
}

} // namespace kernel::display::gui_panel
