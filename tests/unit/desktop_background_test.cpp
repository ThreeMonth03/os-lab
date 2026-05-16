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
