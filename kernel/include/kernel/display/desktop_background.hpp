#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"

namespace kernel::display::desktop_background
{

inline constexpr SurfaceId kSurfaceId = 1;

struct SolidColorSource
{
    Color color;
};

[[nodiscard]] Rect bounds_for(uint64_t surface_width, uint64_t surface_height);
[[nodiscard]] Rect repaint_region(Rect bounds, Rect dirty_rect);
void paint_solid(Surface & surface, Rect bounds, SolidColorSource source, Rect dirty_rect);

} // namespace kernel::display::desktop_background
