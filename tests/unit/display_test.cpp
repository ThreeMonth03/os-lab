#include <stdint.h>
#include <gtest/gtest.h>
#include "kernel/display/backing_surface.hpp"
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

TEST(DisplayTest, ComputesIntersectRect)
{
    expect_rect(kernel::display::intersect_rect({10, 5, 4, 4}, {12, 8, 8, 2}), 12, 8, 2, 1);
    expect_rect(kernel::display::intersect_rect({0, 0, 3, 3}, {3, 0, 2, 2}), 0, 0, 0, 0);
    expect_rect(kernel::display::intersect_rect({}, {1, 2, 3, 4}), 0, 0, 0, 0);
    expect_rect(kernel::display::intersect_rect({8, 8, 8, 8}, {0, 0, 10, 10}), 8, 8, 2, 2);
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

TEST(DisplayTest, BackingSurfaceUsesAbsoluteBounds)
{
    uint32_t pixels[12] = {};
    kernel::display::BackingSurface surface(pixels, {10, 20, 4, 3}, 4);

    EXPECT_TRUE(surface.ready());
    EXPECT_TRUE(surface.contains(10, 20));
    EXPECT_TRUE(surface.contains(13, 22));
    EXPECT_FALSE(surface.contains(9, 20));
    EXPECT_FALSE(surface.contains(14, 22));

    surface.put_pixel(11, 21, {7});
    EXPECT_EQ(surface.pixel(11, 21).value, 7u);
    EXPECT_EQ(pixels[5], 7u);
    EXPECT_FALSE(surface.sample(9, 20).opaque());
    ASSERT_TRUE(surface.sample(11, 21).opaque());
    EXPECT_EQ(surface.sample(11, 21).color.value, 7u);
}

TEST(DisplayTest, ComputesBackingSurfaceStorageBytes)
{
    size_t bytes = 0;

    ASSERT_TRUE(kernel::display::backing_surface_required_bytes({10, 20, 4, 3}, bytes));
    EXPECT_EQ(bytes, 48u);

    EXPECT_FALSE(kernel::display::backing_surface_required_bytes({}, bytes));
}

TEST(DisplayTest, BackingSurfaceFillRectIsClipped)
{
    uint32_t pixels[12] = {};
    kernel::display::BackingSurface surface(pixels, {10, 20, 4, 3}, 4);

    surface.fill_rect({12, 21, 8, 8}, {5});

    EXPECT_EQ(surface.pixel(12, 21).value, 5u);
    EXPECT_EQ(surface.pixel(13, 21).value, 5u);
    EXPECT_EQ(surface.pixel(12, 22).value, 5u);
    EXPECT_EQ(surface.pixel(13, 22).value, 5u);
    EXPECT_EQ(surface.pixel(11, 21).value, 0u);
}

} // namespace
