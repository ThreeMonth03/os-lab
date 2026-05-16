#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/gui_surface.hpp"

namespace kernel::display::gui_panel
{

inline constexpr GuiSurfaceId kGuiSurfaceId = 1;
inline constexpr uint64_t kDefaultWidth = 180;
inline constexpr uint64_t kDefaultHeight = 56;
inline constexpr uint64_t kDefaultMargin = 16;
inline constexpr uint64_t kDefaultPadding = 6;

struct Config
{
    uint64_t width = kDefaultWidth;
    uint64_t height = kDefaultHeight;
    uint64_t margin = kDefaultMargin;
    bool visible = false;
};

[[nodiscard]] Config default_config();
[[nodiscard]] Rect bounds_for(uint64_t surface_width, uint64_t surface_height, Config config = {});
[[nodiscard]] Rect content_bounds(Rect panel_bounds, uint64_t padding = kDefaultPadding);
[[nodiscard]] GuiSurface make_surface(uint64_t surface_width,
                                      uint64_t surface_height,
                                      GuiSurfaceId id = kGuiSurfaceId,
                                      Config config = {});
[[nodiscard]] bool should_redraw(const GuiSurface & surface);

} // namespace kernel::display::gui_panel
