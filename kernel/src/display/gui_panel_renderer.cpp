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
