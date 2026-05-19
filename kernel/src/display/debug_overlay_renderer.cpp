#include "kernel/display/debug_overlay_renderer.hpp"

#include "kernel/text/font5x7.hpp"

namespace
{

constexpr uint64_t kPadding = 2;
constexpr uint64_t kGlyphSpacing = 1;
constexpr uint64_t kLineHeight = kernel::text::Glyph5x7::height + 2;

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

bool contains(kernel::display::Rect rect, uint64_t x, uint64_t y)
{
    return !rect.empty() && x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

uint64_t line_pixel_width(const char * line)
{
    if (line == nullptr || *line == '\0')
    {
        return 0;
    }

    uint64_t width = 0;
    while (*line != '\0')
    {
        width += kernel::text::Glyph5x7::width;
        ++line;
        if (*line != '\0')
        {
            width += kGlyphSpacing;
        }
    }
    return width;
}

uint64_t line_start_x(kernel::display::Rect overlay_bounds,
                      const char * line,
                      kernel::display::debug_overlay::StatusTextAlignment alignment)
{
    const uint64_t left = overlay_bounds.x + kPadding;
    if (alignment == kernel::display::debug_overlay::StatusTextAlignment::Left ||
        overlay_bounds.width <= kPadding * 2)
    {
        return left;
    }

    const uint64_t right = overlay_bounds.x + overlay_bounds.width - kPadding;
    const uint64_t available_width = right > left ? right - left : 0;
    const uint64_t visible_width = min_u64(line_pixel_width(line), available_width);
    return right - visible_width;
}

uint64_t line_y_for(kernel::display::Rect overlay_bounds, uint64_t line_index)
{
    return overlay_bounds.y + kPadding + (line_index * kLineHeight);
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

template <typename SurfaceType>
void paint_region_impl(SurfaceType & surface,
                       kernel::display::Rect overlay_bounds,
                       const kernel::display::debug_overlay::Lines & lines,
                       kernel::display::debug_overlay::Palette palette,
                       kernel::display::debug_overlay::StatusTextAlignment alignment,
                       kernel::display::Rect dirty_rect)
{
    if (!surface.ready())
    {
        return;
    }

    const kernel::display::Rect region =
        kernel::display::debug_overlay::repaint_region(overlay_bounds, dirty_rect);
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
            surface.put_pixel(x,
                              y,
                              kernel::display::debug_overlay::pixel_color_at(overlay_bounds,
                                                                             lines,
                                                                             palette,
                                                                             alignment,
                                                                             x,
                                                                             y));
        }
    }
}

} // namespace

namespace kernel::display::debug_overlay
{

Rect repaint_region(Rect overlay_bounds, Rect dirty_rect)
{
    return intersect_rect(overlay_bounds, dirty_rect);
}

Rect text_line_bounds(StatusTextLayout layout, const char * line, uint64_t line_index)
{
    if (layout.overlay_bounds.empty() || line == nullptr || *line == '\0' ||
        layout.overlay_bounds.width <= kPadding * 2)
    {
        return {};
    }

    const uint64_t text_y = line_y_for(layout.overlay_bounds, line_index);
    const uint64_t bottom = layout.overlay_bounds.y + layout.overlay_bounds.height;
    if (text_y + kernel::text::Glyph5x7::height > bottom)
    {
        return {};
    }

    const uint64_t available_width = layout.overlay_bounds.width - (kPadding * 2);
    const uint64_t width = min_u64(line_pixel_width(line), available_width);
    if (width == 0)
    {
        return {};
    }

    return {line_start_x(layout.overlay_bounds, line, layout.alignment),
            text_y,
            width,
            kernel::text::Glyph5x7::height};
}

Color pixel_color_at(Rect overlay_bounds,
                     const Lines & lines,
                     Palette palette,
                     StatusTextAlignment alignment,
                     uint64_t x,
                     uint64_t y)
{
    const uint64_t text_x = line_start_x(overlay_bounds, lines.first, alignment);
    uint64_t text_y = line_y_for(overlay_bounds, 0);
    const uint64_t bottom = overlay_bounds.y + overlay_bounds.height;
    if (text_y + kernel::text::Glyph5x7::height <= bottom &&
        line_pixel_is_foreground(lines.first, text_x, text_y, overlay_bounds, x, y))
    {
        return palette.foreground;
    }

    text_y = line_y_for(overlay_bounds, 1);
    const uint64_t second_text_x = line_start_x(overlay_bounds, lines.second, alignment);
    if (text_y + kernel::text::Glyph5x7::height <= bottom &&
        line_pixel_is_foreground(lines.second, second_text_x, text_y, overlay_bounds, x, y))
    {
        return palette.foreground;
    }

    return palette.background;
}

Color pixel_color_at(Rect overlay_bounds, const Lines & lines, Palette palette, uint64_t x, uint64_t y)
{
    return pixel_color_at(overlay_bounds, lines, palette, StatusTextAlignment::Left, x, y);
}

void paint_region(Surface & surface,
                  Rect overlay_bounds,
                  const Lines & lines,
                  Palette palette,
                  StatusTextAlignment alignment,
                  Rect dirty_rect)
{
    paint_region_impl(surface, overlay_bounds, lines, palette, alignment, dirty_rect);
}

void paint_region(BackingSurface & surface,
                  Rect overlay_bounds,
                  const Lines & lines,
                  Palette palette,
                  StatusTextAlignment alignment,
                  Rect dirty_rect)
{
    paint_region_impl(surface, overlay_bounds, lines, palette, alignment, dirty_rect);
}

void paint_region(Surface & surface, Rect overlay_bounds, const Lines & lines, Palette palette, Rect dirty_rect)
{
    paint_region(surface, overlay_bounds, lines, palette, StatusTextAlignment::Left, dirty_rect);
}

void paint_region(BackingSurface & surface, Rect overlay_bounds, const Lines & lines, Palette palette, Rect dirty_rect)
{
    paint_region(surface, overlay_bounds, lines, palette, StatusTextAlignment::Left, dirty_rect);
}

} // namespace kernel::display::debug_overlay
