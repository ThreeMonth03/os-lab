#include <gtest/gtest.h>

#include "kernel/display/terminal_renderer.hpp"

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

} // namespace

TEST(TerminalRendererTest, ClearScreenOnlyClearsViewport)
{
    constexpr uint64_t width = 32;
    constexpr uint64_t height = 24;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    kernel::display::TerminalRenderer renderer;
    renderer.reset(surface, {4, 3, 20, 18}, {1}, {2});

    renderer.clear_screen();

    EXPECT_EQ(surface.pixel(0, 0).value, 9u);
    EXPECT_EQ(surface.pixel(4, 3).value, 2u);
    EXPECT_EQ(surface.pixel(23, 20).value, 2u);
    EXPECT_EQ(surface.pixel(24, 20).value, 9u);
    EXPECT_EQ(surface.pixel(4, 21).value, 9u);
}

TEST(TerminalRendererTest, DrawGlyphUsesViewportOrigin)
{
    constexpr uint64_t width = 32;
    constexpr uint64_t height = 24;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    kernel::display::TerminalRenderer renderer;
    renderer.reset(surface, {4, 3, 20, 18}, {1}, {2});

    renderer.draw_glyph('A', 0, 0);

    EXPECT_EQ(surface.pixel(0, 0).value, 9u);
    EXPECT_EQ(surface.pixel(4, 3).value, 2u);
    EXPECT_EQ(surface.pixel(8, 3).value, 1u);
}

TEST(TerminalRendererTest, ScrollRowsOnlyScrollsViewport)
{
    constexpr uint64_t width = 20;
    constexpr uint64_t height = 48;
    uint32_t pixels[width * height] = {};
    for (uint64_t y = 0; y < height; ++y)
    {
        for (uint64_t x = 0; x < width; ++x)
        {
            pixels[(y * width) + x] = static_cast<uint32_t>((y * 100) + x);
        }
    }

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    kernel::display::TerminalRenderer renderer;
    renderer.reset(surface,
                   {2, 2, 12, kernel::display::TerminalRenderer::kCellHeight * 2},
                   {1},
                   {0});

    renderer.scroll_up_rows(1);

    EXPECT_EQ(surface.pixel(0, 2).value, 200u);
    EXPECT_EQ(surface.pixel(2, 2).value,
              static_cast<uint32_t>((2 + kernel::display::TerminalRenderer::kCellHeight) * 100 + 2));
    EXPECT_EQ(surface.pixel(13, 2).value,
              static_cast<uint32_t>((2 + kernel::display::TerminalRenderer::kCellHeight) * 100 + 13));
    EXPECT_EQ(surface.pixel(14, 2).value, 214u);
    EXPECT_EQ(surface.pixel(2, 2 + kernel::display::TerminalRenderer::kCellHeight).value, 0u);
}
