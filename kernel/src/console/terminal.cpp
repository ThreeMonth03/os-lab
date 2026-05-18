#include "kernel/console/terminal.hpp"

#include "terminal_app.hpp"

#include "../display/display_runtime_app.hpp"

namespace
{

namespace display = kernel::display;

kernel::console::TerminalApp g_terminal_app;

kernel::display::PixelSample sample_terminal_pixel(uint64_t x, uint64_t y)
{
    return g_terminal_app.sample_pixel(x, y);
}

const uint32_t * terminal_row_pixels(uint64_t y)
{
    return g_terminal_app.row_pixels(y);
}

kernel::display::PixelSample sample_terminal_caret_pixel(uint64_t x, uint64_t y)
{
    return g_terminal_app.sample_caret_pixel(x, y);
}

kernel::display::Rect terminal_caret_bounds()
{
    return g_terminal_app.caret_bounds();
}

bool resize_terminal_app(kernel::display::AppSurface surface)
{
    return g_terminal_app.resize(surface);
}

void sync_terminal_app_state(kernel::display::AppSurface surface)
{
    g_terminal_app.sync_surface_state(surface);
}

} // namespace

namespace kernel::console::terminal
{

bool init()
{
    if (!display::runtime::init(TerminalApp::kCellWidth, TerminalApp::kCellHeight))
    {
        return false;
    }

    const display::runtime::AppSurfaceHostConfig config = display::runtime::primary_app_config();
    const TerminalRepaintSink repaint_sink{display::runtime::submit_app_surface_damage};
    if (!config.valid() ||
        !g_terminal_app.reset(config.surface, config.foreground, config.background, repaint_sink))
    {
        return false;
    }

    if (!display::runtime::register_app_surface_pixel_source(sample_terminal_pixel) ||
        !display::runtime::register_app_surface_row_source(terminal_row_pixels) ||
        !display::runtime::register_app_surface_scroll_composition(
            display::LayerScrollComposition::RecomposeFromSource) ||
        !display::runtime::register_app_surface_resize_callback(resize_terminal_app) ||
        !display::runtime::register_app_surface_state_callback(sync_terminal_app_state) ||
        !display::runtime::register_text_caret(sample_terminal_caret_pixel, terminal_caret_bounds))
    {
        return false;
    }

    clear();
    display::runtime::refresh_desktop();
    return true;
}

ScopedUpdate::ScopedUpdate()
{
    if (!ready())
    {
        return;
    }

    display::runtime::begin_frame();
    g_terminal_app.begin_update();
    active_ = true;
}

ScopedUpdate::~ScopedUpdate()
{
    if (!active_)
    {
        return;
    }

    g_terminal_app.end_update();
    display::runtime::end_frame();
}

bool ready()
{
    return display::runtime::ready() && g_terminal_app.ready();
}

uint64_t columns()
{
    return g_terminal_app.columns();
}

uint64_t rows()
{
    return g_terminal_app.rows();
}

uint64_t cursor_column()
{
    return g_terminal_app.cursor_column();
}

uint64_t cursor_row()
{
    return g_terminal_app.cursor_row();
}

void clear()
{
    g_terminal_app.clear();
}

void clear_cell_at(uint64_t column, uint64_t row)
{
    g_terminal_app.clear_cell_at(column, row);
}

void clear_row_from(uint64_t column, uint64_t row)
{
    g_terminal_app.clear_row_from(column, row);
}

void draw_char_at(uint64_t column, uint64_t row, char value)
{
    g_terminal_app.draw_char_at(column, row, value);
}

void set_row_continuation(uint64_t row, bool continuation)
{
    g_terminal_app.set_row_continuation(row, continuation);
}

void set_cursor(uint64_t column, uint64_t row)
{
    g_terminal_app.set_cursor(column, row);
}

void show_cursor()
{
    g_terminal_app.show_cursor();
}

void hide_cursor()
{
    g_terminal_app.hide_cursor();
}

void write_char(char value)
{
    g_terminal_app.write_char(value);
}

void write_string(StringView value)
{
    ScopedUpdate update;
    g_terminal_app.write_string(value);
}

void write_string(const char * value)
{
    ScopedUpdate update;
    g_terminal_app.write_string(value);
}

void write_line(StringView value)
{
    ScopedUpdate update;
    g_terminal_app.write_line(value);
}

void write_line(const char * value)
{
    ScopedUpdate update;
    g_terminal_app.write_line(value);
}

void write_hex(uint64_t value)
{
    ScopedUpdate update;
    g_terminal_app.write_hex(value);
}

void write_decimal(uint64_t value)
{
    ScopedUpdate update;
    g_terminal_app.write_decimal(value);
}

} // namespace kernel::console::terminal
