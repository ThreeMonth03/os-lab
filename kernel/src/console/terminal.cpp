#include "kernel/console/terminal.hpp"

#include "terminal_app.hpp"

#include "kernel/display/display_runtime.hpp"

namespace
{

namespace display = kernel::display;

kernel::console::TerminalApp g_terminal_app;

void repaint_terminal_text_region(kernel::display::Rect dirty_rect)
{
    g_terminal_app.repaint_region(dirty_rect);
}

} // namespace

namespace kernel::console::terminal
{

bool init()
{
    if (!display::runtime::init(TerminalApp::kCellWidth,
                                TerminalApp::kCellHeight,
                                repaint_terminal_text_region))
    {
        return false;
    }

    const display::runtime::TerminalAppConfig config = display::runtime::terminal_app_config();
    const TerminalRepaintSink repaint_sink{
        display::runtime::repaint_layers_above_terminal_app,
    };
    if (!config.valid() ||
        !g_terminal_app.reset(*config.surface,
                              config.app_surface,
                              config.foreground,
                              config.background,
                              repaint_sink))
    {
        return false;
    }

    display::runtime::refresh_desktop();
    clear();
    return true;
}

ScopedUpdate::ScopedUpdate()
{
    if (!ready())
    {
        return;
    }

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

kernel::display::HitTestResult pointer_target()
{
    return display::runtime::pointer_target();
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
    g_terminal_app.write_string(value);
}

void write_string(const char * value)
{
    g_terminal_app.write_string(value);
}

void write_line(StringView value)
{
    g_terminal_app.write_line(value);
}

void write_line(const char * value)
{
    g_terminal_app.write_line(value);
}

void write_hex(uint64_t value)
{
    g_terminal_app.write_hex(value);
}

void write_decimal(uint64_t value)
{
    g_terminal_app.write_decimal(value);
}

void update_pointer_target(uint64_t x, uint64_t y)
{
    display::runtime::update_pointer_target(x, y);
}

} // namespace kernel::console::terminal
