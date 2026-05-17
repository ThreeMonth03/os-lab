#pragma once

#include "kernel/display/backing_surface.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/gui_surface.hpp"

namespace kernel::display::gui_panel
{

struct Palette
{
    Color border;
    Color background;
    Color foreground;
};

[[nodiscard]] Rect repaint_region(const GuiSurface & panel, Rect dirty_rect);
[[nodiscard]] PixelSample sample_pixel(const GuiSurface & panel,
                                       Palette palette,
                                       uint64_t x,
                                       uint64_t y);
void paint_region(Surface & surface, const GuiSurface & panel, Palette palette, Rect dirty_rect);

} // namespace kernel::display::gui_panel
