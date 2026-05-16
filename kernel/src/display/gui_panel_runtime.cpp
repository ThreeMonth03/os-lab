#include "kernel/display/gui_panel.hpp"

#include "kernel/display/compositor.hpp"
#include "kernel/display/gui_panel_renderer.hpp"

namespace
{

struct PanelState
{
    kernel::display::Surface * surface = nullptr;
    kernel::display::GuiSurface panel;
    kernel::display::Rect desktop_bounds;
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
    if (!kernel::display::gui_panel::ready())
    {
        return;
    }

    const kernel::display::Rect desktop_region =
        kernel::display::intersect_rect(g_state.desktop_bounds, dirty_rect);
    if (desktop_region.empty())
    {
        return;
    }

    g_state.surface->fill_rect(desktop_region, g_state.palette.background);
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
    g_state.desktop_bounds = {0, 0, surface.width(), surface.height()};
    g_state.palette = {border, background, foreground};
    g_state.config = config;
    g_state.initialized = true;
    (void)compositor::register_layer_repaint_callback(LayerKind::DesktopPanel, repaint_panel);
    return true;
}

bool ready()
{
    return g_state.initialized && g_state.surface != nullptr && g_state.surface->ready();
}

void refresh_now()
{
    if (!ready())
    {
        return;
    }

    repaint_panel(g_state.desktop_bounds);
    compositor::mark_dirty(g_state.desktop_bounds);
    compositor::repaint_layers_above(LayerKind::DesktopPanel, g_state.desktop_bounds);
}

} // namespace kernel::display::gui_panel
