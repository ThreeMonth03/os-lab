#include "desktop_background_runtime.hpp"

#include "kernel/display/composited_surface.hpp"
#include "kernel/display/compositor.hpp"

namespace
{

struct DesktopBackgroundState
{
    kernel::display::Surface * surface = nullptr;
    kernel::display::Rect bounds;
    kernel::display::desktop_background::BackgroundSource source;
    bool initialized = false;
};

DesktopBackgroundState g_state;

bool background_ready()
{
    return g_state.initialized && g_state.surface != nullptr && g_state.surface->ready();
}

void repaint_background(kernel::display::Rect dirty_rect)
{
    if (!background_ready())
    {
        return;
    }

    kernel::display::desktop_background::paint(*g_state.surface,
                                               g_state.bounds,
                                               g_state.source,
                                               dirty_rect);
}

} // namespace

namespace kernel::display::desktop_background
{

bool init(Surface & surface, Rect bounds, BackgroundSource source)
{
    g_state = {};
    if (!surface.ready() || bounds.empty())
    {
        return false;
    }

    const CompositedSurfaceDescriptor background_surface =
        make_composited_surface(kSurfaceId, CompositedSurfaceRole::Background, bounds);
    if (!compositor::register_surface(background_surface) ||
        !compositor::register_layer_repaint_callback(LayerKind::DesktopBackground,
                                                     repaint_background))
    {
        return false;
    }

    g_state.surface = &surface;
    g_state.bounds = bounds;
    g_state.source = source;
    g_state.initialized = true;
    return true;
}

} // namespace kernel::display::desktop_background
