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

struct StatusTextLayout
{
    Rect overlay_bounds;
    StatusTextAlignment alignment = StatusTextAlignment::Left;
};

[[nodiscard]] Rect repaint_region(Rect overlay_bounds, Rect dirty_rect);
[[nodiscard]] Rect text_line_bounds(StatusTextLayout layout, const char * line, uint64_t line_index);
[[nodiscard]] Color pixel_color_at(Rect overlay_bounds,
                                   const Lines & lines,
                                   Palette palette,
                                   StatusTextAlignment alignment,
                                   uint64_t x,
                                   uint64_t y);
[[nodiscard]] Color pixel_color_at(Rect overlay_bounds,
                                   const Lines & lines,
                                   Palette palette,
                                   uint64_t x,
                                   uint64_t y);
void paint_region(BackingSurface & surface,
                  Rect overlay_bounds,
                  const Lines & lines,
                  Palette palette,
                  StatusTextAlignment alignment,
                  Rect dirty_rect);
void paint_region(Surface & surface,
                  Rect overlay_bounds,
                  const Lines & lines,
                  Palette palette,
                  StatusTextAlignment alignment,
                  Rect dirty_rect);
void paint_region(BackingSurface & surface, Rect overlay_bounds, const Lines & lines, Palette palette, Rect dirty_rect);
void paint_region(Surface & surface, Rect overlay_bounds, const Lines & lines, Palette palette, Rect dirty_rect);

} // namespace kernel::display::debug_overlay
