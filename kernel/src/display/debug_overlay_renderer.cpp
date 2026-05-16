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

bool glyph_pixel_is_set(char value, uint64_t glyph_column, uint64_t glyph_row)
{
    const kernel::text::Glyph5x7 & glyph = kernel::text::Font5x7::glyph_for(value);
    const auto mask = static_cast<uint8_t>(1u << (kernel::text::Glyph5x7::width - glyph_column - 1));
    return (glyph.rows[glyph_row] & mask) != 0;
}

bool line_pixel_is_foreground(const char * line,
                              uint64_t line_x,
                              uint64_t line_y,
                              kernel::display::Rect overlay_bounds,
                              uint64_t pixel_x,
                              uint64_t pixel_y)
{
    const uint64_t right = overlay_bounds.x + overlay_bounds.width;
    uint64_t glyph_x = line_x;
    while (*line != '\0' && glyph_x + kernel::text::Glyph5x7::width <= right)
    {
        const kernel::display::Rect glyph_bounds{glyph_x,
                                                 line_y,
                                                 kernel::text::Glyph5x7::width,
                                                 kernel::text::Glyph5x7::height};
        if (contains(glyph_bounds, pixel_x, pixel_y))
        {
            return glyph_pixel_is_set(*line, pixel_x - glyph_x, pixel_y - line_y);
        }
        glyph_x += kernel::text::Glyph5x7::width + kGlyphSpacing;
        ++line;
    }
    return false;
}

} // namespace

namespace kernel::display::debug_overlay
{

Rect repaint_region(Rect overlay_bounds, Rect dirty_rect)
{
    return intersect_rect(overlay_bounds, dirty_rect);
}

Color pixel_color_at(Rect overlay_bounds, const Lines & lines, Palette palette, uint64_t x, uint64_t y)
{
    const uint64_t text_x = overlay_bounds.x + kPadding;
    uint64_t text_y = overlay_bounds.y + kPadding;
    const uint64_t bottom = overlay_bounds.y + overlay_bounds.height;
    if (text_y + kernel::text::Glyph5x7::height <= bottom &&
        line_pixel_is_foreground(lines.first, text_x, text_y, overlay_bounds, x, y))
    {
        return palette.foreground;
    }

    text_y += kLineHeight;
    if (text_y + kernel::text::Glyph5x7::height <= bottom &&
        line_pixel_is_foreground(lines.second, text_x, text_y, overlay_bounds, x, y))
    {
        return palette.foreground;
    }

    return palette.background;
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

    for (uint64_t row = 0; row < region.height; ++row)
    {
        const uint64_t y = region.y + row;
        for (uint64_t column = 0; column < region.width; ++column)
        {
            const uint64_t x = region.x + column;
            surface.put_pixel(x, y, pixel_color_at(overlay_bounds, lines, palette, x, y));
        }
    }
}

} // namespace kernel::display::debug_overlay
