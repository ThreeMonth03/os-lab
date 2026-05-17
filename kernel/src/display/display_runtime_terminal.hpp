#pragma once

#include <stdint.h>

#include "kernel/display/app_surface.hpp"
#include "kernel/display/compositor.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/frame_damage.hpp"

namespace kernel::display::runtime
{

struct TerminalAppConfig
{
    AppSurface app_surface;
    Color foreground;
    Color background;

    bool valid() const { return app_surface.valid(); }
};

[[nodiscard]] bool init(uint64_t terminal_cell_width, uint64_t terminal_cell_height);

[[nodiscard]] bool ready();
[[nodiscard]] TerminalAppConfig terminal_app_config();

void refresh_desktop();
void submit_terminal_app_damage(FrameDamage damage);
[[nodiscard]] bool register_terminal_app_pixel_source(compositor::LayerPixelCallback callback);
[[nodiscard]] bool register_terminal_caret(compositor::LayerPixelCallback pixel_callback,
                                           compositor::LayerBoundsCallback bounds_callback);

} // namespace kernel::display::runtime
