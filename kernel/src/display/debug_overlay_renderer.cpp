#include "kernel/display/debug_overlay_renderer.hpp"

#include "kernel/text/font5x7.hpp"

namespace
{

constexpr uint64_t kPadding = 2;
constexpr uint64_t kGlyphSpacing = 1;
constexpr uint64_t kLineHeight = kernel::text::Glyph5x7::height + 2;

bool contains(kernel::display::Rect rect, uint64_t x, uint64_t y)
{
    return !rect.empty() && x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

void draw_glyph(kernel::display::Surface & surface,
                char value,
                uint64_t x,
                uint64_t y,
                kernel::display::Color color,
                kernel::display::Rect dirty_region)
{
    const kernel::text::Glyph5x7 & glyph = kernel::text::Font5x7::glyph_for(value);
    for (uint64_t glyph_row = 0; glyph_row < kernel::text::Glyph5x7::height; ++glyph_row)
    {
        for (uint64_t glyph_column = 0; glyph_column < kernel::text::Glyph5x7::width; ++glyph_column)
        {
            const auto mask = static_cast<uint8_t>(1u << (kernel::text::Glyph5x7::width - glyph_column - 1));
            const uint64_t pixel_x = x + glyph_column;
            const uint64_t pixel_y = y + glyph_row;
            if ((glyph.rows[glyph_row] & mask) != 0 && contains(dirty_region, pixel_x, pixel_y))
            {
                surface.put_pixel(pixel_x, pixel_y, color);
            }
        }
    }
}

void draw_line(kernel::display::Surface & surface,
               const char * line,
               uint64_t x,
               uint64_t y,
               kernel::display::Rect overlay_bounds,
               kernel::display::Color color,
               kernel::display::Rect dirty_region)
{
    const uint64_t right = overlay_bounds.x + overlay_bounds.width;
    while (*line != '\0' && x + kernel::text::Glyph5x7::width <= right)
    {
        const kernel::display::Rect glyph_bounds{x,
                                                 y,
                                                 kernel::text::Glyph5x7::width,
                                                 kernel::text::Glyph5x7::height};
        if (!kernel::display::intersect_rect(glyph_bounds, dirty_region).empty())
        {
            draw_glyph(surface, *line, x, y, color, dirty_region);
        }
        x += kernel::text::Glyph5x7::width + kGlyphSpacing;
        ++line;
    }
}

} // namespace

namespace kernel::display::debug_overlay
{

Rect repaint_region(Rect overlay_bounds, Rect dirty_rect)
{
    return intersect_rect(overlay_bounds, dirty_rect);
}

void paint_region(Surface & surface, Rect overlay_bounds, const Lines & lines, Palette palette, Rect dirty_rect)
{
    if (!surface.ready())
    {
        return;
    }

    const Rect region = repaint_region(overlay_bounds, dirty_rect);
    if (region.empty())
    {
        return;
    }

    surface.fill_rect(region, palette.background);

    const uint64_t text_x = overlay_bounds.x + kPadding;
    uint64_t text_y = overlay_bounds.y + kPadding;
    const uint64_t bottom = overlay_bounds.y + overlay_bounds.height;
    if (text_y + kernel::text::Glyph5x7::height <= bottom)
    {
        draw_line(surface, lines.first, text_x, text_y, overlay_bounds, palette.foreground, region);
    }

    text_y += kLineHeight;
    if (text_y + kernel::text::Glyph5x7::height <= bottom)
    {
        draw_line(surface, lines.second, text_x, text_y, overlay_bounds, palette.foreground, region);
    }
}

} // namespace kernel::display::debug_overlay
