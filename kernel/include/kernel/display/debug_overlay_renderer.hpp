#pragma once

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
void paint_region(Surface & surface, Rect overlay_bounds, const Lines & lines, Palette palette, Rect dirty_rect);

} // namespace kernel::display::debug_overlay
