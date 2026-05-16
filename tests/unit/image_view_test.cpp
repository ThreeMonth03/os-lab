#include <stdint.h>
#include <gtest/gtest.h>

#include "kernel/display/image_view.hpp"

namespace
{

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

} // namespace

TEST(ImageViewTest, RejectsEmptyNullAndUndersizedStride)
{
    constexpr uint8_t pixels[16] = {};

    EXPECT_FALSE(kernel::display::ImageView().valid());
    EXPECT_FALSE(
        kernel::display::ImageView(nullptr, 2, 2, 8, kernel::display::PixelFormat::Rgba8888)
            .valid());
    EXPECT_FALSE(kernel::display::ImageView(pixels,
                                            0,
                                            2,
                                            8,
                                            kernel::display::PixelFormat::Rgba8888)
                     .valid());
    EXPECT_FALSE(kernel::display::ImageView(pixels,
                                            2,
                                            2,
                                            7,
                                            kernel::display::PixelFormat::Rgba8888)
                     .valid());
}

TEST(ImageViewTest, ReadsRgbaAndBgraPixels)
{
    constexpr uint8_t rgba[] = {
        0x10,
        0x20,
        0x30,
        0xff,
        0x40,
        0x50,
        0x60,
        0x80,
    };
    constexpr uint8_t bgra[] = {
        0x30,
        0x20,
        0x10,
        0xff,
        0x60,
        0x50,
        0x40,
        0x80,
    };

    const kernel::display::ImageView rgba_image(rgba,
                                                2,
                                                1,
                                                8,
                                                kernel::display::PixelFormat::Rgba8888);
    const kernel::display::ImageView bgra_image(bgra,
                                                2,
                                                1,
                                                8,
                                                kernel::display::PixelFormat::Bgra8888);

    EXPECT_EQ(rgba_image.pixel(0, 0), (kernel::display::RgbaColor{0x10, 0x20, 0x30, 0xff}));
    EXPECT_EQ(rgba_image.pixel(1, 0), (kernel::display::RgbaColor{0x40, 0x50, 0x60, 0x80}));
    EXPECT_EQ(bgra_image.pixel(0, 0), (kernel::display::RgbaColor{0x10, 0x20, 0x30, 0xff}));
    EXPECT_EQ(bgra_image.pixel(1, 0), (kernel::display::RgbaColor{0x40, 0x50, 0x60, 0x80}));
}

TEST(ImageViewTest, ComputesBlitRegionFromImageSurfaceAndDirtyRect)
{
    constexpr uint8_t pixels[4 * 4 * 4] = {};
    const kernel::display::ImageView image(pixels,
                                           4,
                                           4,
                                           16,
                                           kernel::display::PixelFormat::Rgba8888);

    expect_rect(kernel::display::image_blit_region(image,
                                                   {0, 0, 10, 10},
                                                   3,
                                                   2,
                                                   {5, 4, 10, 10}),
                5,
                4,
                2,
                2);
    EXPECT_TRUE(kernel::display::image_blit_region(image,
                                                   {0, 0, 10, 10},
                                                   3,
                                                   2,
                                                   {8, 4, 1, 1})
                    .empty());
}

TEST(ImageViewTest, BlitsOnlyClippedImagePixels)
{
    constexpr uint64_t surface_width = 5;
    constexpr uint64_t surface_height = 4;
    uint32_t surface_pixels[surface_width * surface_height] = {};
    for (uint32_t & pixel : surface_pixels)
    {
        pixel = 0x111111u;
    }

    constexpr uint8_t image_pixels[] = {
        0x10,
        0x00,
        0x00,
        0xff,
        0x00,
        0x20,
        0x00,
        0xff,
        0x00,
        0x00,
        0x30,
        0xff,
        0x40,
        0x50,
        0x60,
        0xff,
    };

    kernel::display::Surface surface(surface_pixels,
                                     surface_width,
                                     surface_height,
                                     surface_width * sizeof(uint32_t));
    const kernel::display::ImageView image(image_pixels,
                                           2,
                                           2,
                                           8,
                                           kernel::display::PixelFormat::Rgba8888);

    kernel::display::blit_image(surface,
                                image,
                                2,
                                1,
                                {3, 1, 3, 3},
                                kernel::display::xrgb8888_color_layout());

    EXPECT_EQ(surface.pixel(2, 1).value, 0x111111u);
    EXPECT_EQ(surface.pixel(3, 1).value, 0x002000u);
    EXPECT_EQ(surface.pixel(2, 2).value, 0x111111u);
    EXPECT_EQ(surface.pixel(3, 2).value, 0x405060u);
    EXPECT_EQ(surface.pixel(4, 2).value, 0x111111u);
}
