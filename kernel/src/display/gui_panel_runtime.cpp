#include "kernel/display/gui_panel.hpp"

#include "kernel/display/compositor.hpp"
#include "kernel/text/font5x7.hpp"

namespace
{

constexpr const char * kTitle = "GUI PANEL";
constexpr uint64_t kGlyphSpacing = 1;

struct PanelState
{
    kernel::display::Surface * surface = nullptr;
    kernel::display::GuiSurface panel;
    kernel::display::Color border;
    kernel::display::Color background;
    kernel::display::Color foreground;
    kernel::display::gui_panel::Config config;
    bool initialized = false;
};

PanelState g_state;

void draw_glyph(char value, uint64_t x, uint64_t y)
{
    if (g_state.surface == nullptr || !g_state.surface->ready())
    {
        return;
    }

    const kernel::Glyph5x7 & glyph = kernel::Font5x7::glyph_for(value);
    for (uint64_t glyph_row = 0; glyph_row < kernel::Glyph5x7::height; ++glyph_row)
    {
        for (uint64_t glyph_column = 0; glyph_column < kernel::Glyph5x7::width; ++glyph_column)
        {
            const uint8_t mask = static_cast<uint8_t>(1u << (kernel::Glyph5x7::width - glyph_column - 1));
            if ((glyph.rows[glyph_row] & mask) != 0)
            {
                g_state.surface->put_pixel(x + glyph_column, y + glyph_row, g_state.foreground);
            }
        }
    }
}

void draw_text(const char * text, kernel::display::Rect bounds)
{
    if (g_state.surface == nullptr || !g_state.surface->ready())
    {
        return;
    }

    uint64_t x = bounds.x;
    const uint64_t right = bounds.x + bounds.width;
    while (*text != '\0' && x + kernel::Glyph5x7::width <= right)
    {
        draw_glyph(*text, x, bounds.y);
        x += kernel::Glyph5x7::width + kGlyphSpacing;
        ++text;
    }
}

void draw_border(kernel::display::Rect bounds)
{
    if (g_state.surface == nullptr || !g_state.surface->ready() || bounds.empty())
    {
        return;
    }

    g_state.surface->fill_rect({bounds.x, bounds.y, bounds.width, 1}, g_state.border);
    g_state.surface->fill_rect({bounds.x, bounds.y + bounds.height - 1, bounds.width, 1}, g_state.border);
    g_state.surface->fill_rect({bounds.x, bounds.y, 1, bounds.height}, g_state.border);
    g_state.surface->fill_rect({bounds.x + bounds.width - 1, bounds.y, 1, bounds.height}, g_state.border);
}

void paint_panel()
{
    if (!kernel::display::gui_panel::ready() ||
        !kernel::display::gui_panel::should_redraw(g_state.panel))
    {
        return;
    }

    g_state.surface->fill_rect(g_state.panel.bounds, g_state.background);
    draw_border(g_state.panel.bounds);
    draw_text(kTitle, kernel::display::gui_panel::content_bounds(g_state.panel.bounds));
}

void repaint_panel(kernel::display::Rect dirty_rect)
{
    if (!kernel::display::rects_overlap(g_state.panel.bounds, dirty_rect))
    {
        return;
    }

    paint_panel();
}

} // namespace

namespace kernel::display::gui_panel
{

bool init(Surface & surface, const GuiSurface & panel, Color border, Color background, Color foreground, Config config)
{
    g_state = {};
    if (!surface.ready() || !panel.valid())
    {
        return false;
    }

    g_state.surface = &surface;
    g_state.panel = panel;
    g_state.border = border;
    g_state.background = background;
    g_state.foreground = foreground;
    g_state.config = config;
    g_state.initialized = true;
    (void)compositor::register_layer_repaint_callback(LayerKind::GuiSurface, repaint_panel);
    return true;
}

bool ready()
{
    return g_state.initialized && g_state.surface != nullptr && g_state.surface->ready();
}

void refresh_now()
{
    if (!ready() || !should_redraw(g_state.panel))
    {
        return;
    }

    paint_panel();
    compositor::mark_dirty(g_state.panel.bounds);
    compositor::repaint_layers_above(LayerKind::GuiSurface, g_state.panel.bounds);
}

} // namespace kernel::display::gui_panel
