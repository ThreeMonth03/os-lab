#include "kernel/console/terminal.hpp"

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/terminal_renderer.hpp"
#include "kernel/boot/limine_support.hpp"
#include "kernel/display/mouse_cursor.hpp"
#include "kernel/text/text_console.hpp"

namespace {

constexpr uint64_t kCellWidth = kernel::display::TerminalRenderer::kCellWidth;
constexpr uint64_t kCellHeight = kernel::display::TerminalRenderer::kCellHeight;

struct TerminalState {
    kernel::display::Surface surface;
    kernel::display::TerminalRenderer renderer;
    kernel::TextConsole console;
    uint64_t visible_cursor_column = 0;
    uint64_t visible_cursor_row = 0;
    bool cursor_visible = false;
};

TerminalState g_state;

class MouseCursorGuard {
  public:
    MouseCursorGuard() { kernel::mouse_cursor::hide(); }
    ~MouseCursorGuard() { kernel::mouse_cursor::show(); }

    MouseCursorGuard(const MouseCursorGuard&) = delete;
    MouseCursorGuard& operator=(const MouseCursorGuard&) = delete;
};

uint32_t pack_rgb(const limine_framebuffer& framebuffer, uint8_t red, uint8_t green, uint8_t blue) {
    return (static_cast<uint32_t>(red) << framebuffer.red_mask_shift) |
           (static_cast<uint32_t>(green) << framebuffer.green_mask_shift) |
           (static_cast<uint32_t>(blue) << framebuffer.blue_mask_shift);
}

void hide_text_cursor() {
    if (!g_state.cursor_visible) {
        return;
    }

    g_state.renderer.erase_cursor(g_state.visible_cursor_column, g_state.visible_cursor_row);
    g_state.cursor_visible = false;
}

void apply_console_update(kernel::TextConsoleUpdate update) {
    switch (update.action) {
    case kernel::TextConsoleAction::DrawGlyph:
        g_state.renderer.draw_glyph(update.glyph, update.cell.column, update.cell.row);
        break;
    case kernel::TextConsoleAction::ClearCell:
        g_state.renderer.clear_cell(update.cell.column, update.cell.row);
        break;
    case kernel::TextConsoleAction::None:
        break;
    }

    if (update.scroll) {
        g_state.renderer.scroll();
    }
}

} // namespace

namespace kernel::terminal {

bool init() {
    const auto* response = boot::framebuffer();
    if (response == nullptr || response->framebuffer_count == 0) {
        return false;
    }

    auto* framebuffer = response->framebuffers[0];
    if (framebuffer == nullptr || framebuffer->bpp != 32 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB || framebuffer->width < kCellWidth ||
        framebuffer->height < kCellHeight) {
        return false;
    }

    g_state.surface = kernel::display::Surface(framebuffer->address, framebuffer->width,
                                               framebuffer->height, framebuffer->pitch);
    g_state.console.reset(framebuffer->width / kCellWidth, framebuffer->height / kCellHeight);
    const kernel::display::Color foreground{pack_rgb(*framebuffer, 0xf5, 0xf5, 0xf5)};
    const kernel::display::Color background{pack_rgb(*framebuffer, 0x10, 0x14, 0x1c)};
    g_state.renderer.reset(g_state.surface, foreground, background);

    clear();
    return true;
}

bool ready() { return g_state.surface.ready(); }

uint64_t columns() { return g_state.console.columns(); }

uint64_t rows() { return g_state.console.rows(); }

uint64_t cursor_column() { return g_state.console.cursor_column(); }

uint64_t cursor_row() { return g_state.console.cursor_row(); }

void clear() {
    if (!ready()) {
        return;
    }

    g_state.cursor_visible = false;
    MouseCursorGuard mouse_cursor;
    g_state.renderer.clear_screen();
    g_state.console.clear();
}

void clear_cell_at(uint64_t column, uint64_t row) {
    if (!ready() || column >= g_state.console.columns() || row >= g_state.console.rows()) {
        return;
    }

    MouseCursorGuard mouse_cursor;
    g_state.renderer.clear_cell(column, row);
}

void clear_row_from(uint64_t column, uint64_t row) {
    if (!ready() || row >= g_state.console.rows()) {
        return;
    }

    MouseCursorGuard mouse_cursor;
    while (column < g_state.console.columns()) {
        g_state.renderer.clear_cell(column, row);
        ++column;
    }
}

void draw_char_at(uint64_t column, uint64_t row, char value) {
    if (!ready() || column >= g_state.console.columns() || row >= g_state.console.rows()) {
        return;
    }

    MouseCursorGuard mouse_cursor;
    g_state.renderer.draw_glyph(value, column, row);
}

void set_cursor(uint64_t column, uint64_t row) {
    if (!ready()) {
        return;
    }

    g_state.console.set_cursor(column, row);
}

void show_cursor() {
    if (!ready()) {
        return;
    }

    MouseCursorGuard mouse_cursor;
    hide_text_cursor();
    g_state.renderer.draw_cursor(g_state.console.cursor_column(), g_state.console.cursor_row());
    g_state.visible_cursor_column = g_state.console.cursor_column();
    g_state.visible_cursor_row = g_state.console.cursor_row();
    g_state.cursor_visible = true;
}

void hide_cursor() {
    if (!ready() || !g_state.cursor_visible) {
        return;
    }

    MouseCursorGuard mouse_cursor;
    hide_text_cursor();
}

void write_char(char value) {
    if (!ready()) {
        return;
    }

    switch (value) {
    case '\t':
        for (int index = 0; index < 4; ++index) {
            write_char(' ');
        }
        return;
    default:
        break;
    }

    MouseCursorGuard mouse_cursor;
    switch (value) {
    case '\n':
        apply_console_update(g_state.console.newline());
        return;
    case '\r':
        g_state.console.carriage_return();
        return;
    case '\b':
        apply_console_update(g_state.console.backspace());
        return;
    default:
        break;
    }

    apply_console_update(g_state.console.write_char(value));
}

void write_string(StringView value) {
    for (char character : value) {
        write_char(character);
    }
}

void write_string(const char* value) { write_string(StringView(value)); }

void write_line(StringView value) {
    write_string(value);
    write_char('\n');
}

void write_line(const char* value) { write_line(StringView(value)); }

void write_hex(uint64_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    write_string("0x");

    for (int shift = 60; shift >= 0; shift -= 4) {
        const auto nibble = static_cast<uint8_t>((value >> shift) & 0xf);
        write_char(digits[nibble]);
    }
}

void write_decimal(uint64_t value) {
    char buffer[21] = {};
    size_t index = 0;

    if (value == 0) {
        write_char('0');
        return;
    }

    while (value > 0) {
        buffer[index++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        write_char(buffer[--index]);
    }
}

} // namespace kernel::terminal
