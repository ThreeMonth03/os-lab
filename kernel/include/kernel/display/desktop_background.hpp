#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/image_view.hpp"

namespace kernel::display::desktop_background
{

inline constexpr SurfaceId kSurfaceId = 1;

struct SolidColorSource
{
    Color color;
};

struct WallpaperSource
{
    ImageView image;
    ColorLayout color_layout;
    uint64_t x = 0;
    uint64_t y = 0;

    bool valid() const { return image.valid(); }
};

struct BackgroundSource
{
    SolidColorSource solid;
    WallpaperSource wallpaper;

    bool has_wallpaper() const { return wallpaper.valid(); }
};

[[nodiscard]] BackgroundSource solid_background(Color color);
[[nodiscard]] BackgroundSource wallpaper_background(Color color,
                                                    ImageView image,
                                                    ColorLayout color_layout);
[[nodiscard]] Rect bounds_for(uint64_t surface_width, uint64_t surface_height);
[[nodiscard]] Rect repaint_region(Rect bounds, Rect dirty_rect);
void paint(Surface & surface, Rect bounds, BackgroundSource source, Rect dirty_rect);
void paint_solid(Surface & surface, Rect bounds, SolidColorSource source, Rect dirty_rect);

} // namespace kernel::display::desktop_background
