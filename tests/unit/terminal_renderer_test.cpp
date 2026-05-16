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
