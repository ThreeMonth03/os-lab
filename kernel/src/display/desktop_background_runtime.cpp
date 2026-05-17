#include "desktop_background_runtime.hpp"

#include "kernel/display/composited_surface.hpp"
#include "kernel/display/compositor.hpp"

namespace
{

struct DesktopBackgroundState
{
    kernel::display::Rect bounds;
    kernel::display::desktop_background::BackgroundSource source;
    bool initialized = false;
};

DesktopBackgroundState g_state;

bool background_ready()
{
    return g_state.initialized;
}

kernel::display::PixelSample sample_background_pixel(uint64_t x, uint64_t y)
{
    if (!background_ready())
    {
        return kernel::display::transparent_pixel();
    }

    return kernel::display::desktop_background::sample(g_state.source, g_state.bounds, x, y);
}

} // namespace

namespace kernel::display::desktop_background
{

bool init(Rect bounds, BackgroundSource source)
{
    g_state = {};
    if (bounds.empty())
    {
        return false;
    }

    const CompositedSurfaceDescriptor background_surface =
        make_composited_surface(kSurfaceId, CompositedSurfaceRole::Background, bounds);
    if (!compositor::register_surface(background_surface) ||
        !compositor::register_layer_pixel_callback(LayerKind::DesktopBackground,
                                                   sample_background_pixel))
    {
        return false;
    }

    g_state.bounds = bounds;
    g_state.source = source;
    g_state.initialized = true;
    return true;
}

} // namespace kernel::display::desktop_background
