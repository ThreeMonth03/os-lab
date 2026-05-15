#include "kernel/display/compositor.hpp"

#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/gui_panel.hpp"
#include "kernel/display/mouse_cursor.hpp"

namespace
{

kernel::display::Compositor g_compositor;

} // namespace

namespace kernel::display::compositor
{

void init(Rect bounds)
{
    g_compositor.reset(bounds);
}

bool register_layer(Layer layer)
{
    return g_compositor.register_layer(layer);
}

void mark_dirty(Rect rect)
{
    (void)g_compositor.mark_dirty(rect);
}

size_t dirty_count()
{
    return g_compositor.dirty_count();
}

bool pop_dirty(Rect & rect)
{
    return g_compositor.pop_dirty(rect);
}

void repaint_layers_above(LayerKind updated_layer, Rect dirty_rect)
{
    const Layer * gui_surface = g_compositor.find_layer(LayerKind::GuiSurface);
    if (gui_surface != nullptr && should_repaint_layer_after_update(*gui_surface, updated_layer, dirty_rect))
    {
        gui_panel::refresh_now();
    }

    const Layer * overlay_layer = g_compositor.find_layer(LayerKind::DebugOverlay);
    if (overlay_layer != nullptr && should_repaint_layer_after_update(*overlay_layer, updated_layer, dirty_rect))
    {
        debug_overlay::refresh_now();
    }
}

RedrawGuard::RedrawGuard(Rect dirty_rect)
{
    mark_dirty(dirty_rect);
    mouse_cursor::hide();
}

RedrawGuard::~RedrawGuard()
{
    mouse_cursor::show();
}

} // namespace kernel::display::compositor
