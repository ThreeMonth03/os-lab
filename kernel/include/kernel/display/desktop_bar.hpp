#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/gui_surface.hpp"

namespace kernel::display::desktop_bar
{

inline constexpr GuiSurfaceId kGuiSurfaceId = 1;
inline constexpr uint64_t kDefaultHeight = 32;
inline constexpr uint64_t kMinimumHeight = 8;
inline constexpr uint64_t kTopBorderHeight = 1;

struct Config
{
    uint64_t height = kDefaultHeight;
    bool visible = false;
};

struct Palette
{
    Color border;
    Color background;
};

struct Layout
{
    Rect desktop_bounds;
    Rect bar_bounds;
    Rect work_area;
    bool bar_visible = false;
};

[[nodiscard]] Config default_config();
[[nodiscard]] Rect bounds_for(Rect desktop_bounds, Config config = {});
[[nodiscard]] Rect work_area_for(Rect desktop_bounds, Rect bar_bounds, bool bar_visible);
[[nodiscard]] Layout layout_for(Rect desktop_bounds, Config config = {});
[[nodiscard]] GuiSurface make_surface(Rect desktop_bounds, GuiSurfaceId id = kGuiSurfaceId, Config config = {});
[[nodiscard]] bool should_redraw(const GuiSurface & surface);
[[nodiscard]] PixelSample sample_pixel(const GuiSurface & surface, Palette palette, uint64_t x, uint64_t y);

} // namespace kernel::display::desktop_bar
