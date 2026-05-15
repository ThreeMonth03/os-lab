#include <stdint.h>
#include <gtest/gtest.h>
#include "kernel/display/display.hpp"

namespace
{

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

TEST(DisplayTest, ClipsRectToSurfaceBounds)
{
    expect_rect(kernel::display::clip_rect({1, 2, 3, 4}, 10, 10), 1, 2, 3, 4);
    expect_rect(kernel::display::clip_rect({8, 7, 5, 6}, 10, 10), 8, 7, 2, 3);
    expect_rect(kernel::display::clip_rect({10, 0, 1, 1}, 10, 10), 10, 0, 0, 0);
    expect_rect(kernel::display::clip_rect({0, 10, 1, 1}, 10, 10), 0, 10, 0, 0);
}

TEST(DisplayTest, ComputesBoundingRect)
{
    expect_rect(kernel::display::bounding_rect({10, 5, 4, 4}, {12, 8, 8, 2}), 10, 5, 10, 5);
    expect_rect(kernel::display::bounding_rect({}, {1, 2, 3, 4}), 1, 2, 3, 4);
    expect_rect(kernel::display::bounding_rect({5, 6, 7, 8}, {}), 5, 6, 7, 8);
}

TEST(DisplayTest, PutPixelAndFillRectAreClipped)
{
    uint32_t pixels[12] = {};
    kernel::display::Surface surface(pixels, 4, 3, 4 * sizeof(uint32_t));

    surface.put_pixel(1, 1, {3});
    surface.put_pixel(9, 9, {7});
    EXPECT_EQ(pixels[5], 3u);
    EXPECT_EQ(surface.pixel(1, 1).value, 3u);
    EXPECT_EQ(surface.pixel(9, 9).value, 0u);

    surface.fill_rect({2, 1, 9, 9}, {5});
    EXPECT_EQ(pixels[6], 5u);
    EXPECT_EQ(pixels[7], 5u);
    EXPECT_EQ(pixels[10], 5u);
    EXPECT_EQ(pixels[11], 5u);
    EXPECT_EQ(pixels[0], 0u);
}

TEST(DisplayTest, ScrollUpMovesPixelsAndClearsBottomRows)
{
    uint32_t pixels[12] = {
        10,
        11,
        12,
        13,
        20,
        21,
        22,
        23,
        30,
        31,
        32,
        33,
    };
    kernel::display::Surface surface(pixels, 4, 3, 4 * sizeof(uint32_t));

    surface.scroll_up(1, {0});

    EXPECT_EQ(pixels[0], 20u);
    EXPECT_EQ(pixels[1], 21u);
    EXPECT_EQ(pixels[4], 30u);
    EXPECT_EQ(pixels[5], 31u);
    EXPECT_EQ(pixels[8], 0u);
    EXPECT_EQ(pixels[11], 0u);
}

} // namespace
