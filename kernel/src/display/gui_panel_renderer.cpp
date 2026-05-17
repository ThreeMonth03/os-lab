#include "kernel/display/gui_panel_renderer.hpp"

#include "kernel/display/gui_panel.hpp"
#include "kernel/text/font5x7.hpp"

namespace
{

constexpr const char * kTitle = "GUI PANEL";
constexpr uint64_t kGlyphSpacing = 1;

bool contains(kernel::display::Rect rect, uint64_t x, uint64_t y)
{
    return !rect.empty() && x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

void fill_region(kernel::display::Surface & surface,
                 kernel::display::Rect rect,
                 kernel::display::Color color,
                 kernel::display::Rect dirty_region)
{
    const kernel::display::Rect clipped = kernel::display::intersect_rect(rect, dirty_region);
    if (!clipped.empty())
    {
        surface.fill_rect(clipped, color);
    }
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

void draw_text(kernel::display::Surface & surface,
               const char * text,
               kernel::display::Rect bounds,
               kernel::display::Color color,
               kernel::display::Rect dirty_region)
{
    uint64_t x = bounds.x;
    const uint64_t right = bounds.x + bounds.width;
    while (*text != '\0' && x + kernel::text::Glyph5x7::width <= right)
    {
        draw_glyph(surface, *text, x, bounds.y, color, dirty_region);
        x += kernel::text::Glyph5x7::width + kGlyphSpacing;
        ++text;
    }
}

bool glyph_pixel_set(char value, uint64_t glyph_column, uint64_t glyph_row)
{
    if (glyph_column >= kernel::text::Glyph5x7::width ||
        glyph_row >= kernel::text::Glyph5x7::height)
    {
        return false;
    }

    const kernel::text::Glyph5x7 & glyph = kernel::text::Font5x7::glyph_for(value);
    const auto mask = static_cast<uint8_t>(1u << (kernel::text::Glyph5x7::width - glyph_column - 1));
    return (glyph.rows[glyph_row] & mask) != 0;
}

bool text_pixel_set(const char * text, kernel::display::Rect bounds, uint64_t x, uint64_t y)
{
    if (!contains(bounds, x, y))
    {
        return false;
    }

    uint64_t glyph_x = bounds.x;
    const uint64_t right = bounds.x + bounds.width;
    while (*text != '\0' && glyph_x + kernel::text::Glyph5x7::width <= right)
    {
        const kernel::display::Rect glyph_bounds{
            glyph_x,
            bounds.y,
            kernel::text::Glyph5x7::width,
            kernel::text::Glyph5x7::height,
        };
        if (contains(glyph_bounds, x, y))
        {
            return glyph_pixel_set(*text, x - glyph_x, y - bounds.y);
        }

        glyph_x += kernel::text::Glyph5x7::width + kGlyphSpacing;
        ++text;
    }
    return false;
}

bool border_pixel(kernel::display::Rect bounds, uint64_t x, uint64_t y)
{
    return contains(bounds, x, y) &&
           (x == bounds.x || y == bounds.y || x == bounds.x + bounds.width - 1 ||
            y == bounds.y + bounds.height - 1);
}

void draw_border(kernel::display::Surface & surface,
                 kernel::display::Rect bounds,
                 kernel::display::Color color,
                 kernel::display::Rect dirty_region)
{
    if (bounds.empty())
    {
        return;
    }

    fill_region(surface, {bounds.x, bounds.y, bounds.width, 1}, color, dirty_region);
    fill_region(surface, {bounds.x, bounds.y + bounds.height - 1, bounds.width, 1}, color, dirty_region);
    fill_region(surface, {bounds.x, bounds.y, 1, bounds.height}, color, dirty_region);
    fill_region(surface, {bounds.x + bounds.width - 1, bounds.y, 1, bounds.height}, color, dirty_region);
}

} // namespace

namespace kernel::display::gui_panel
{

Rect repaint_region(const GuiSurface & panel, Rect dirty_rect)
{
    if (!should_redraw(panel))
    {
        return {};
    }

    return intersect_rect(panel.bounds, dirty_rect);
}

PixelSample sample_pixel(const GuiSurface & panel, Palette palette, uint64_t x, uint64_t y)
{
    if (!should_redraw(panel) || !contains(panel.bounds, x, y))
    {
        return transparent_pixel();
    }

    if (border_pixel(panel.bounds, x, y))
    {
        return opaque_pixel(palette.border);
    }

    if (text_pixel_set(kTitle, content_bounds(panel.bounds), x, y))
    {
        return opaque_pixel(palette.foreground);
    }

    return opaque_pixel(palette.background);
}

void paint_region(Surface & surface, const GuiSurface & panel, Palette palette, Rect dirty_rect)
{
    if (!surface.ready())
    {
        return;
    }

    const Rect region = repaint_region(panel, dirty_rect);
    if (region.empty())
    {
        return;
    }

    surface.fill_rect(region, palette.background);
    draw_border(surface, panel.bounds, palette.border, region);
    draw_text(surface, kTitle, content_bounds(panel.bounds), palette.foreground, region);
}

} // namespace kernel::display::gui_panel
