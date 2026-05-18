#pragma once

#include <stdint.h>

#include "kernel/display/app_surface.hpp"
#include "kernel/display/compositor.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/frame_damage.hpp"

namespace kernel::display::runtime
{

struct AppSurfaceHostConfig
{
    AppSurface surface;
    Color foreground;
    Color background;

    bool valid() const { return surface.valid(); }
};

using AppSurfaceResizeCallback = bool (*)(AppSurface surface);
using AppSurfaceStateCallback = void (*)(AppSurface surface);

[[nodiscard]] bool init(uint64_t terminal_cell_width, uint64_t terminal_cell_height);

[[nodiscard]] bool ready();
[[nodiscard]] AppSurfaceHostConfig primary_app_config();
[[nodiscard]] bool resize_app_surface(AppSurfaceId id, Rect bounds);
[[nodiscard]] bool set_app_surface_visible(AppSurfaceId id, bool visible);
[[nodiscard]] bool clear_app_surface_focus(AppSurfaceId id);
[[nodiscard]] bool focus_app_surface(AppSurfaceId id);
[[nodiscard]] bool close_app_surface(AppSurfaceId id);

void begin_frame();
void end_frame();
void refresh_desktop();
void submit_app_surface_damage(FrameDamage damage);
[[nodiscard]] bool register_app_surface_resize_callback(AppSurfaceResizeCallback callback);
[[nodiscard]] bool register_app_surface_state_callback(AppSurfaceStateCallback callback);
[[nodiscard]] bool register_app_surface_pixel_source(compositor::LayerPixelCallback callback);
[[nodiscard]] bool register_app_surface_row_source(compositor::LayerRowCallback callback);
[[nodiscard]] bool register_app_surface_scroll_composition(LayerScrollComposition composition);
[[nodiscard]] bool register_text_caret(compositor::LayerPixelCallback pixel_callback,
                                       compositor::LayerBoundsCallback bounds_callback);

} // namespace kernel::display::runtime
