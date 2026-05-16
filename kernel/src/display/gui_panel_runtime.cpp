#include "kernel/display/gui_panel.hpp"

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

void paint_panel_region(kernel::display::Rect dirty_rect)
{
    if (!kernel::display::gui_panel::ready() ||
        !kernel::display::gui_panel::should_redraw(g_state.panel))
    {
        return;
    }

    kernel::display::gui_panel::paint_region(*g_state.surface,
                                             g_state.panel,
                                             g_state.palette,
                                             dirty_rect);
}

void repaint_panel(kernel::display::Rect dirty_rect)
{
    if (!kernel::display::rects_overlap(g_state.panel.bounds, dirty_rect))
    {
        return;
    }

    paint_panel_region(dirty_rect);
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
    g_state.initialized = true;
    (void)compositor::register_layer_repaint_callback(LayerKind::GuiSurface, repaint_panel);
    return true;
}

bool ready()
{
    return g_state.initialized && g_state.surface != nullptr && g_state.surface->ready();
}

void refresh_now()
{
    if (!ready() || !should_redraw(g_state.panel))
    {
        return;
    }

    paint_panel_region(g_state.panel.bounds);
    compositor::mark_dirty(g_state.panel.bounds);
    compositor::repaint_layers_above(LayerKind::GuiSurface, g_state.panel.bounds);
}

} // namespace kernel::display::gui_panel
