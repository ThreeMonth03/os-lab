#include <gtest/gtest.h>

#include <stdint.h>

#include "kernel/display/scroll_mapped_surface.hpp"

namespace
{

constexpr kernel::display::Color color(uint32_t value)
{
    return {value};
}

} // namespace

TEST(ScrollMappedSurfaceTest, SamplesPixelsThroughScrollOffset)
{
    uint32_t pixels[16] = {};
    kernel::display::BackingSurface storage(pixels, {0, 0, 4, 4}, 4);
    kernel::display::ScrollMappedSurface surface(storage, {0, 0, 4, 4});

    for (uint64_t y = 0; y < 4; ++y)
    {
        for (uint64_t x = 0; x < 4; ++x)
        {
            storage.put_pixel(x, y, color(static_cast<uint32_t>((y * 10) + x)));
        }
    }

    surface.scroll_up(1);

    EXPECT_EQ(surface.pixel(0, 0).value, 10u);
    EXPECT_EQ(surface.pixel(3, 0).value, 13u);
    EXPECT_EQ(surface.pixel(0, 2).value, 30u);
    EXPECT_EQ(surface.pixel(0, 3).value, 0u);
}

TEST(ScrollMappedSurfaceTest, WritesUseMappedRowsAfterScroll)
{
    uint32_t pixels[16] = {};
    kernel::display::BackingSurface storage(pixels, {0, 0, 4, 4}, 4);
    kernel::display::ScrollMappedSurface surface(storage, {0, 0, 4, 4});

    surface.scroll_up(1);
    surface.put_pixel(1, 3, color(42));

    EXPECT_EQ(surface.pixel(1, 3).value, 42u);
    EXPECT_EQ(storage.pixel(1, 0).value, 42u);
}

TEST(ScrollMappedSurfaceTest, FillRectClearsExposedRowsThroughMapping)
{
    uint32_t pixels[16] = {};
    kernel::display::BackingSurface storage(pixels, {0, 0, 4, 4}, 4);
    kernel::display::ScrollMappedSurface surface(storage, {0, 0, 4, 4});

    storage.fill_rect({0, 0, 4, 4}, color(7));
    surface.scroll_up(1);
    surface.fill_rect({0, 3, 4, 1}, color(0));

    for (uint64_t x = 0; x < 4; ++x)
    {
        EXPECT_EQ(surface.pixel(x, 3).value, 0u);
        EXPECT_EQ(storage.pixel(x, 0).value, 0u);
    }
    EXPECT_EQ(surface.pixel(0, 0).value, 7u);
}

TEST(ScrollMappedSurfaceTest, RowPixelsExposeMappedPhysicalRow)
{
    uint32_t pixels[16] = {};
    kernel::display::BackingSurface storage(pixels, {0, 0, 4, 4}, 4);
    kernel::display::ScrollMappedSurface surface(storage, {0, 0, 4, 4});

    for (uint64_t y = 0; y < 4; ++y)
    {
        for (uint64_t x = 0; x < 4; ++x)
        {
            storage.put_pixel(x, y, color(static_cast<uint32_t>((y * 10) + x)));
        }
    }

    surface.scroll_up(2);

    const uint32_t * top_row = surface.row_pixels(0);
    ASSERT_NE(top_row, nullptr);
    EXPECT_EQ(top_row[0], 20u);
    EXPECT_EQ(top_row[3], 23u);

    const uint32_t * wrapped_row = surface.row_pixels(2);
    ASSERT_NE(wrapped_row, nullptr);
    EXPECT_EQ(wrapped_row[0], 0u);
}

TEST(ScrollMappedSurfaceTest, MovePreservesScrollOffsetForSameSizedRegion)
{
    uint32_t pixels[16] = {};
    kernel::display::BackingSurface storage(pixels, {0, 0, 4, 4}, 4);
    kernel::display::ScrollMappedSurface surface(storage, {0, 0, 4, 4});

    for (uint64_t y = 0; y < 4; ++y)
    {
        for (uint64_t x = 0; x < 4; ++x)
        {
            storage.put_pixel(x, y, color(static_cast<uint32_t>((y * 10) + x)));
        }
    }

    surface.scroll_up(1);
    ASSERT_TRUE(storage.move_to({20, 30, 4, 4}));
    EXPECT_TRUE(surface.reset_preserving_scroll(storage, {20, 30, 4, 4}));

    EXPECT_EQ(surface.scroll_offset(), 1u);
    EXPECT_EQ(surface.pixel(20, 30).value, 10u);
    EXPECT_EQ(surface.pixel(23, 30).value, 13u);
    EXPECT_EQ(surface.pixel(20, 33).value, 0u);
}

TEST(ScrollMappedSurfaceTest, PixelsOutsideScrollRegionRemainDirectMapped)
{
    uint32_t pixels[25] = {};
    kernel::display::BackingSurface storage(pixels, {0, 0, 5, 5}, 5);
    kernel::display::ScrollMappedSurface surface(storage, {1, 1, 3, 3});

    storage.put_pixel(0, 0, color(11));
    storage.put_pixel(1, 2, color(22));
    storage.put_pixel(1, 3, color(33));

    surface.scroll_up(1);

    EXPECT_EQ(surface.pixel(0, 0).value, 11u);
    EXPECT_EQ(surface.pixel(1, 1).value, 22u);
    EXPECT_EQ(surface.pixel(1, 2).value, 33u);
}
