#pragma once

#include <stdint.h>

#include "kernel/display/app_surface.hpp"
#include "kernel/display/compositor.hpp"
#include "kernel/display/display.hpp"

namespace kernel::display::runtime
{

struct TerminalAppConfig
{
    Surface * surface = nullptr;
    AppSurface app_surface;
    Color foreground;
    Color background;

    bool valid() const { return surface != nullptr && surface->ready() && app_surface.valid(); }
};

[[nodiscard]] bool init(uint64_t terminal_cell_width,
                        uint64_t terminal_cell_height,
                        compositor::LayerRepaintCallback terminal_repaint_callback);

[[nodiscard]] bool ready();
[[nodiscard]] TerminalAppConfig terminal_app_config();

void refresh_desktop();
void repaint_layers_above_terminal_app(Rect rect);

} // namespace kernel::display::runtime
