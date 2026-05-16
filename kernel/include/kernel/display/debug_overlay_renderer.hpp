#pragma once

#include "kernel/display/backing_surface.hpp"
#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/display.hpp"

namespace kernel::display::debug_overlay
{

struct Palette
{
    Color foreground;
    Color background;
};

[[nodiscard]] Rect repaint_region(Rect overlay_bounds, Rect dirty_rect);
[[nodiscard]] Color pixel_color_at(Rect overlay_bounds,
                                   const Lines & lines,
                                   Palette palette,
                                   uint64_t x,
                                   uint64_t y);
void paint_region(BackingSurface & surface, Rect overlay_bounds, const Lines & lines, Palette palette, Rect dirty_rect);
void paint_region(Surface & surface, Rect overlay_bounds, const Lines & lines, Palette palette, Rect dirty_rect);

} // namespace kernel::display::debug_overlay
