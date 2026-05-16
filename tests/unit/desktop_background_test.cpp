#include <gtest/gtest.h>

#include "kernel/display/desktop_background.hpp"

namespace
{

template <size_t Size>
void fill_pixels(uint32_t (&pixels)[Size], uint32_t value)
{
    for (uint32_t & pixel : pixels)
    {
        pixel = value;
    }
}

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

} // namespace

TEST(DesktopBackgroundTest, UsesEntireSurfaceBounds)
{
    expect_rect(kernel::display::desktop_background::bounds_for(320, 200), 0, 0, 320, 200);
    EXPECT_TRUE(kernel::display::desktop_background::bounds_for(0, 200).empty());
    EXPECT_TRUE(kernel::display::desktop_background::bounds_for(320, 0).empty());
}

TEST(DesktopBackgroundTest, ClipsRepaintRegionToBackgroundBounds)
{
    const kernel::display::Rect region =
        kernel::display::desktop_background::repaint_region({0, 0, 100, 50}, {90, 40, 20, 20});

    expect_rect(region, 90, 40, 10, 10);
    EXPECT_TRUE(kernel::display::desktop_background::repaint_region({0, 0, 100, 50},
                                                                    {120, 40, 2, 2})
                    .empty());
}

TEST(DesktopBackgroundTest, PaintsOnlyClippedSolidBackgroundRegion)
{
    constexpr uint64_t width = 16;
    constexpr uint64_t height = 12;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    kernel::display::desktop_background::paint_solid(surface,
                                                     {0, 0, width, height},
                                                     {{7}},
                                                     {4, 3, 5, 2});

    EXPECT_EQ(surface.pixel(3, 3).value, 9u);
    EXPECT_EQ(surface.pixel(4, 3).value, 7u);
    EXPECT_EQ(surface.pixel(8, 4).value, 7u);
    EXPECT_EQ(surface.pixel(9, 4).value, 9u);
}

TEST(DesktopBackgroundTest, PaintsWallpaperOverSolidBackground)
{
    constexpr uint64_t width = 5;
    constexpr uint64_t height = 4;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    constexpr uint8_t wallpaper_pixels[] = {
        0x10,
        0x20,
        0x30,
        0xff,
        0x40,
        0x50,
        0x60,
        0xff,
        0x70,
        0x80,
        0x90,
        0xff,
        0xa0,
        0xb0,
        0xc0,
        0xff,
    };

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    const kernel::display::ImageView wallpaper(wallpaper_pixels,
                                               2,
                                               2,
                                               8,
                                               kernel::display::PixelFormat::Rgba8888);

    kernel::display::desktop_background::paint(
        surface,
        {0, 0, width, height},
        kernel::display::desktop_background::wallpaper_background(
            {0x050505u},
            wallpaper,
            kernel::display::xrgb8888_color_layout()),
        {0, 0, width, height});

    EXPECT_EQ(surface.pixel(0, 0).value, 0x102030u);
    EXPECT_EQ(surface.pixel(1, 0).value, 0x405060u);
    EXPECT_EQ(surface.pixel(0, 1).value, 0x708090u);
    EXPECT_EQ(surface.pixel(1, 1).value, 0xa0b0c0u);
    EXPECT_EQ(surface.pixel(2, 1).value, 0x050505u);
    EXPECT_EQ(surface.pixel(4, 3).value, 0x050505u);
}

TEST(DesktopBackgroundTest, DirtyRegionOutsideWallpaperKeepsSolidFallback)
{
    constexpr uint64_t width = 5;
    constexpr uint64_t height = 4;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    constexpr uint8_t wallpaper_pixels[] = {
        0xff,
        0x00,
        0x00,
        0xff,
    };

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    const kernel::display::ImageView wallpaper(wallpaper_pixels,
                                               1,
                                               1,
                                               4,
                                               kernel::display::PixelFormat::Rgba8888);

    kernel::display::desktop_background::paint(
        surface,
        {0, 0, width, height},
        kernel::display::desktop_background::wallpaper_background(
            {0x222222u},
            wallpaper,
            kernel::display::xrgb8888_color_layout()),
        {3, 2, 1, 1});

    EXPECT_EQ(surface.pixel(0, 0).value, 9u);
    EXPECT_EQ(surface.pixel(3, 2).value, 0x222222u);
}
