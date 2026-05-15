#include "kernel/console/terminal.hpp"

#include <stdint.h>

#include "kernel/display/compositor.hpp"
#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/gui_panel.hpp"
#include "kernel/display/gui_surface.hpp"
#include "kernel/display/hit_test.hpp"
#include "kernel/display/terminal_render_cache.hpp"
#include "kernel/display/terminal_renderer.hpp"
#include "kernel/display/terminal_repaint_state.hpp"
#include "kernel/boot/limine_support.hpp"
#include "kernel/text/text_buffer.hpp"
#include "kernel/text/text_console.hpp"

namespace
{

namespace display = kernel::display;
namespace debug_overlay = kernel::display::debug_overlay;
namespace gui_panel = kernel::display::gui_panel;

constexpr uint64_t kCellWidth = display::TerminalRenderer::kCellWidth;
constexpr uint64_t kCellHeight = display::TerminalRenderer::kCellHeight;

struct TerminalState
{
    display::Surface surface;
    display::DisplayTargetRegistry targets;
    display::GuiSurfaceRegistry gui_surfaces;
    display::HitTestResult pointer_target;
    display::TerminalRenderer renderer;
    kernel::TextConsole console;
    kernel::TextBuffer text_buffer;
    display::TerminalRenderCache render_cache;
    display::TerminalRepaintState repaint;
    uint64_t visible_cursor_column = 0;
    uint64_t visible_cursor_row = 0;
    bool cursor_visible = false;
};

TerminalState g_state;

uint32_t pack_rgb(const limine_framebuffer & framebuffer, uint8_t red, uint8_t green, uint8_t blue)
{
    return (static_cast<uint32_t>(red) << framebuffer.red_mask_shift) |
           (static_cast<uint32_t>(green) << framebuffer.green_mask_shift) |
           (static_cast<uint32_t>(blue) << framebuffer.blue_mask_shift);
}

display::Rect terminal_bounds()
{
    return {0, 0, g_state.surface.width(), g_state.surface.height()};
}

bool terminal_ready()
{
    return g_state.surface.ready() && g_state.targets.active_target().valid();
}

display::Rect cell_rect(uint64_t column, uint64_t row)
{
    return {column * kCellWidth, row * kCellHeight, kCellWidth, kCellHeight};
}

display::Rect row_tail_rect(uint64_t column, uint64_t row)
{
    if (column >= g_state.console.columns())
    {
        return {};
    }

    return {column * kCellWidth,
            row * kCellHeight,
            (g_state.console.columns() - column) * kCellWidth,
            kCellHeight};
}

void hide_text_cursor()
{
    if (!g_state.cursor_visible)
    {
        return;
    }

    g_state.renderer.erase_cursor(g_state.visible_cursor_column, g_state.visible_cursor_row);
    g_state.cursor_visible = false;
}

void repaint_text_layer()
{
    g_state.renderer.clear_screen();
    for (uint64_t row = 0; row < g_state.text_buffer.rows(); ++row)
    {
        for (uint64_t column = 0; column < g_state.text_buffer.columns(); ++column)
        {
            const char glyph = g_state.text_buffer.glyph_at(column, row);
            if (glyph != kernel::kTextBufferBlank)
            {
                g_state.renderer.draw_glyph(glyph, column, row);
            }
        }
    }
    g_state.render_cache.synchronize_from(g_state.text_buffer);
}

display::Rect render_dirty_text_cells()
{
    display::Rect dirty_rect;
    for (uint64_t row = 0; row < g_state.text_buffer.rows(); ++row)
    {
        for (uint64_t column = 0; column < g_state.text_buffer.columns(); ++column)
        {
            if (!g_state.render_cache.needs_render(g_state.text_buffer, column, row))
            {
                continue;
            }

            const char glyph = g_state.text_buffer.glyph_at(column, row);
            if (glyph == kernel::kTextBufferBlank)
            {
                g_state.renderer.clear_cell(column, row);
            }
            else
            {
                g_state.renderer.draw_glyph(glyph, column, row);
            }
            (void)g_state.render_cache.mark_rendered(column, row, glyph);
            dirty_rect = display::bounding_rect(dirty_rect, cell_rect(column, row));
        }
    }
    return dirty_rect;
}

display::Rect render_text_repaint(bool full_repaint, uint64_t scroll_rows)
{
    if (full_repaint || !g_state.render_cache.valid())
    {
        repaint_text_layer();
        return terminal_bounds();
    }

    display::Rect dirty_rect;
    if (scroll_rows > 0)
    {
        g_state.renderer.scroll_up_rows(scroll_rows);
        if (!g_state.render_cache.scroll_up(scroll_rows))
        {
            repaint_text_layer();
            return terminal_bounds();
        }
        dirty_rect = terminal_bounds();
    }

    return display::bounding_rect(dirty_rect, render_dirty_text_cells());
}

void repaint_console_region(display::Rect dirty_rect)
{
    if (!terminal_ready() || dirty_rect.empty())
    {
        return;
    }

    for (uint64_t row = 0; row < g_state.text_buffer.rows(); ++row)
    {
        for (uint64_t column = 0; column < g_state.text_buffer.columns(); ++column)
        {
            if (!display::rects_overlap(cell_rect(column, row), dirty_rect))
            {
                continue;
            }

            const char glyph = g_state.text_buffer.glyph_at(column, row);
            if (glyph == kernel::kTextBufferBlank)
            {
                g_state.renderer.clear_cell(column, row);
            }
            else
            {
                g_state.renderer.draw_glyph(glyph, column, row);
            }
            (void)g_state.render_cache.mark_rendered(column, row, glyph);
        }
    }

    if (g_state.cursor_visible &&
        display::rects_overlap(cell_rect(g_state.visible_cursor_column, g_state.visible_cursor_row),
                               dirty_rect))
    {
        g_state.renderer.draw_cursor(g_state.visible_cursor_column, g_state.visible_cursor_row);
    }
}

void apply_repaint_request(display::TerminalRepaintRequest request)
{
    display::Rect dirty_rect = request.dirty_rect;
    if (request.repaint_text_layer)
    {
        dirty_rect = display::bounding_rect(dirty_rect, render_text_repaint(request.full_text_repaint, request.scroll_rows));
        display::compositor::mark_dirty(dirty_rect);
    }

    if (request.repaint_higher_layers && !dirty_rect.empty())
    {
        display::compositor::repaint_layers_above(display::LayerKind::Console, dirty_rect);
    }
}

void apply_repaint_flush(display::TerminalRepaintFlush flush)
{
    display::Rect dirty_rect = flush.dirty_rect;
    if (flush.repaint_text_layer)
    {
        dirty_rect = display::bounding_rect(dirty_rect, render_text_repaint(flush.full_text_repaint, flush.scroll_rows));
        display::compositor::mark_dirty(dirty_rect);
    }

    if (flush.repaint_higher_layers && !dirty_rect.empty())
    {
        display::compositor::repaint_layers_above(display::LayerKind::Console, dirty_rect);
    }
}

void record_console_dirty(display::Rect dirty_rect)
{
    apply_repaint_request(g_state.repaint.record_dirty(dirty_rect));
}

display::Rect apply_console_update(kernel::TextConsoleUpdate update)
{
    display::Rect dirty_rect;
    const bool draw_immediately = !update.scroll && !g_state.repaint.pending_text_repaint() &&
                                  g_state.render_cache.valid();
    switch (update.action)
    {
    case kernel::TextConsoleAction::DrawGlyph:
        g_state.text_buffer.put(update.cell.column, update.cell.row, update.glyph);
        if (draw_immediately)
        {
            g_state.renderer.draw_glyph(update.glyph, update.cell.column, update.cell.row);
            (void)g_state.render_cache.mark_rendered(update.cell.column, update.cell.row, update.glyph);
            dirty_rect = cell_rect(update.cell.column, update.cell.row);
            display::compositor::mark_dirty(dirty_rect);
        }
        break;
    case kernel::TextConsoleAction::ClearCell:
        g_state.text_buffer.clear_cell(update.cell.column, update.cell.row);
        if (draw_immediately)
        {
            g_state.renderer.clear_cell(update.cell.column, update.cell.row);
            (void)g_state.render_cache.mark_rendered(update.cell.column, update.cell.row, kernel::kTextBufferBlank);
            dirty_rect = cell_rect(update.cell.column, update.cell.row);
            display::compositor::mark_dirty(dirty_rect);
        }
        break;
    case kernel::TextConsoleAction::None:
        break;
    }

    if (update.scroll)
    {
        g_state.text_buffer.scroll_up();
        apply_repaint_request(g_state.repaint.record_scroll(terminal_bounds(), g_state.text_buffer.rows()));
        return {};
    }

    return dirty_rect;
}

void register_debug_overlay_target(const limine_framebuffer & framebuffer)
{
    const display::Rect bounds = debug_overlay::bounds_for(framebuffer.width, framebuffer.height);
    if (bounds.empty())
    {
        return;
    }

    const bool registered = g_state.targets.register_target({
        debug_overlay::kSurfaceId,
        display::DisplayTargetKind::DebugOverlay,
        bounds,
        false,
        false,
    });
    if (!registered)
    {
        return;
    }

    const display::SurfaceDescriptor * target = g_state.targets.find(debug_overlay::kSurfaceId);
    if (target == nullptr)
    {
        return;
    }

    const display::Color foreground{pack_rgb(framebuffer, 0x92, 0xf7, 0xb8)};
    const display::Color background{pack_rgb(framebuffer, 0x08, 0x0c, 0x12)};
    (void)debug_overlay::init(g_state.surface, *target, foreground, background);
    (void)display::compositor::register_layer({
        display::LayerKind::DebugOverlay,
        debug_overlay::kSurfaceId,
        bounds,
        true,
    });
}

void register_gui_panel_target(const limine_framebuffer & framebuffer)
{
    const gui_panel::Config config = gui_panel::default_config();
    const display::GuiSurface surface = gui_panel::make_surface(framebuffer.width, framebuffer.height, gui_panel::kGuiSurfaceId, config);
    if (!g_state.gui_surfaces.register_surface(surface))
    {
        return;
    }

    const display::GuiSurface * registered = g_state.gui_surfaces.find(gui_panel::kGuiSurfaceId);
    if (registered == nullptr || !g_state.targets.register_target(registered->display_target()))
    {
        return;
    }

    (void)display::compositor::register_layer(registered->layer());
    const display::Color border{pack_rgb(framebuffer, 0x6b, 0xd6, 0xff)};
    const display::Color background{pack_rgb(framebuffer, 0x12, 0x1b, 0x28)};
    const display::Color foreground{pack_rgb(framebuffer, 0xf5, 0xf5, 0xf5)};
    (void)gui_panel::init(g_state.surface, *registered, border, background, foreground, config);
}

} // namespace

namespace kernel::console::terminal
{

bool init()
{
    const auto * response = boot::framebuffer();
    if (response == nullptr || response->framebuffer_count == 0)
    {
        return false;
    }

    auto * framebuffer = response->framebuffers[0];
    if (framebuffer == nullptr || framebuffer->bpp != 32 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB || framebuffer->width < kCellWidth ||
        framebuffer->height < kCellHeight)
    {
        return false;
    }

    g_state.surface = display::Surface(framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch);
    display::compositor::init({0, 0, framebuffer->width, framebuffer->height});
    g_state.targets.clear();
    g_state.gui_surfaces.clear();
    const bool target_registered = g_state.targets.register_target({
        display::kConsoleSurfaceId,
        display::DisplayTargetKind::Console,
        {0, 0, framebuffer->width, framebuffer->height},
        false,
        false,
    });
    if (!target_registered || !g_state.targets.set_active(display::kConsoleSurfaceId) ||
        !g_state.targets.set_focused(display::kConsoleSurfaceId))
    {
        return false;
    }
    (void)display::compositor::register_layer({
        display::LayerKind::Console,
        display::kConsoleSurfaceId,
        {0, 0, framebuffer->width, framebuffer->height},
        true,
    });
    (void)display::compositor::register_layer_repaint_callback(display::LayerKind::Console,
                                                               repaint_console_region);

    const uint64_t columns = framebuffer->width / kCellWidth;
    const uint64_t rows = framebuffer->height / kCellHeight;
    if (!g_state.text_buffer.reset(columns, rows) || !g_state.render_cache.reset(columns, rows))
    {
        return false;
    }
    g_state.console.reset(columns, rows);
    const display::Color foreground{pack_rgb(*framebuffer, 0xf5, 0xf5, 0xf5)};
    const display::Color background{pack_rgb(*framebuffer, 0x10, 0x14, 0x1c)};
    g_state.renderer.reset(g_state.surface, foreground, background);
    g_state.repaint.reset();
    register_gui_panel_target(*framebuffer);
    register_debug_overlay_target(*framebuffer);

    clear();
    return true;
}

ScopedUpdate::ScopedUpdate()
{
    if (!terminal_ready())
    {
        return;
    }

    g_state.repaint.begin_batch();
    active_ = true;
}

ScopedUpdate::~ScopedUpdate()
{
    if (!active_)
    {
        return;
    }

    const display::TerminalRepaintFlush flush = g_state.repaint.end_batch();
    if (flush.outermost_batch_ended)
    {
        apply_repaint_flush(flush);
    }
}

bool ready() { return terminal_ready(); }

uint64_t columns() { return g_state.console.columns(); }

uint64_t rows() { return g_state.console.rows(); }

uint64_t cursor_column() { return g_state.console.cursor_column(); }

uint64_t cursor_row() { return g_state.console.cursor_row(); }

display::HitTestResult pointer_target() { return g_state.pointer_target; }

void clear()
{
    if (!ready())
    {
        return;
    }

    g_state.cursor_visible = false;
    g_state.renderer.clear_screen();
    g_state.console.clear();
    g_state.text_buffer.clear();
    g_state.render_cache.clear_rendered();
    record_console_dirty(terminal_bounds());
}

void clear_cell_at(uint64_t column, uint64_t row)
{
    if (!ready() || column >= g_state.console.columns() || row >= g_state.console.rows())
    {
        return;
    }

    g_state.text_buffer.clear_cell(column, row);
    g_state.renderer.clear_cell(column, row);
    (void)g_state.render_cache.mark_rendered(column, row, kernel::kTextBufferBlank);
    record_console_dirty(cell_rect(column, row));
}

void clear_row_from(uint64_t column, uint64_t row)
{
    if (!ready() || row >= g_state.console.rows())
    {
        return;
    }

    const display::Rect dirty_rect = row_tail_rect(column, row);
    while (column < g_state.console.columns())
    {
        g_state.text_buffer.clear_cell(column, row);
        g_state.renderer.clear_cell(column, row);
        (void)g_state.render_cache.mark_rendered(column, row, kernel::kTextBufferBlank);
        ++column;
    }
    record_console_dirty(dirty_rect);
}

void draw_char_at(uint64_t column, uint64_t row, char value)
{
    if (!ready() || column >= g_state.console.columns() || row >= g_state.console.rows())
    {
        return;
    }

    g_state.text_buffer.put(column, row, value);
    g_state.renderer.draw_glyph(value, column, row);
    (void)g_state.render_cache.mark_rendered(column, row, value);
    record_console_dirty(cell_rect(column, row));
}

void set_cursor(uint64_t column, uint64_t row)
{
    if (!ready())
    {
        return;
    }

    g_state.console.set_cursor(column, row);
}

void show_cursor()
{
    if (!ready())
    {
        return;
    }

    hide_text_cursor();
    g_state.renderer.draw_cursor(g_state.console.cursor_column(), g_state.console.cursor_row());
    g_state.visible_cursor_column = g_state.console.cursor_column();
    g_state.visible_cursor_row = g_state.console.cursor_row();
    g_state.cursor_visible = true;
    record_console_dirty(cell_rect(g_state.console.cursor_column(), g_state.console.cursor_row()));
}

void hide_cursor()
{
    if (!ready() || !g_state.cursor_visible)
    {
        return;
    }

    hide_text_cursor();
    record_console_dirty(cell_rect(g_state.visible_cursor_column, g_state.visible_cursor_row));
}

void write_char(char value)
{
    if (!ready())
    {
        return;
    }

    switch (value)
    {
    case '\t':
        for (int index = 0; index < 4; ++index)
        {
            write_char(' ');
        }
        return;
    default:
        break;
    }

    display::Rect dirty_rect;
    switch (value)
    {
    case '\n':
        dirty_rect = apply_console_update(g_state.console.newline());
        break;
    case '\r':
        g_state.console.carriage_return();
        break;
    case '\b':
        dirty_rect = apply_console_update(g_state.console.backspace());
        break;
    default:
        dirty_rect = apply_console_update(g_state.console.write_char(value));
        break;
    }
    record_console_dirty(dirty_rect);
}

void write_string(StringView value)
{
    for (char character : value)
    {
        write_char(character);
    }
}

void write_string(const char * value) { write_string(StringView(value)); }

void write_line(StringView value)
{
    write_string(value);
    write_char('\n');
}

void write_line(const char * value) { write_line(StringView(value)); }

void write_hex(uint64_t value)
{
    static constexpr char digits[] = "0123456789abcdef";
    write_string("0x");

    for (int shift = 60; shift >= 0; shift -= 4)
    {
        const auto nibble = static_cast<uint8_t>((value >> shift) & 0xf);
        write_char(digits[nibble]);
    }
}

void write_decimal(uint64_t value)
{
    char buffer[21] = {};
    size_t index = 0;

    if (value == 0)
    {
        write_char('0');
        return;
    }

    while (value > 0)
    {
        buffer[index++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    while (index > 0)
    {
        write_char(buffer[--index]);
    }
}

void update_pointer_target(uint64_t x, uint64_t y)
{
    if (!ready())
    {
        g_state.pointer_target = {};
        return;
    }

    g_state.pointer_target = display::hit_test(g_state.targets, g_state.gui_surfaces, x, y);
}

} // namespace kernel::console::terminal
