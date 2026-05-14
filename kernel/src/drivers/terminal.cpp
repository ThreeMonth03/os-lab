#include "kernel/terminal.hpp"

#include <stdint.h>

#include "kernel/display.hpp"
#include "kernel/limine_support.hpp"
#include "kernel/mouse_cursor.hpp"
#include "kernel/text_console.hpp"

namespace {

constexpr uint64_t kGlyphWidth = 5;
constexpr uint64_t kGlyphHeight = 7;
constexpr uint64_t kGlyphScale = 2;
constexpr uint64_t kCellWidth = (kGlyphWidth + 1) * kGlyphScale;
constexpr uint64_t kCellHeight = (kGlyphHeight + 2) * kGlyphScale;
constexpr uint64_t kCursorTop = (kGlyphHeight * kGlyphScale) + 1;
constexpr uint64_t kCursorHeight = kGlyphScale;

struct Glyph {
    uint8_t rows[kGlyphHeight];
};

struct TerminalState {
    kernel::display::Surface surface;
    kernel::TextConsole console;
    uint64_t visible_cursor_column = 0;
    uint64_t visible_cursor_row = 0;
    uint32_t foreground = 0;
    uint32_t background = 0;
    bool cursor_visible = false;
};

TerminalState g_state;

uint32_t pack_rgb(const limine_framebuffer& framebuffer, uint8_t red, uint8_t green, uint8_t blue) {
    return (static_cast<uint32_t>(red) << framebuffer.red_mask_shift) |
           (static_cast<uint32_t>(green) << framebuffer.green_mask_shift) |
           (static_cast<uint32_t>(blue) << framebuffer.blue_mask_shift);
}

Glyph glyph_for(char value) {
    switch (value) {
    case '0':
        return {{0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}};
    case '1':
        return {{0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e}};
    case '2':
        return {{0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}};
    case '3':
        return {{0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e}};
    case '4':
        return {{0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}};
    case '5':
        return {{0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e}};
    case '6':
        return {{0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e}};
    case '7':
        return {{0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}};
    case '8':
        return {{0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}};
    case '9':
        return {{0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e}};
    case 'A':
        return {{0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}};
    case 'B':
        return {{0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}};
    case 'C':
        return {{0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f}};
    case 'D':
        return {{0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}};
    case 'E':
        return {{0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}};
    case 'F':
        return {{0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}};
    case 'G':
        return {{0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f}};
    case 'H':
        return {{0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}};
    case 'I':
        return {{0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}};
    case 'J':
        return {{0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e}};
    case 'K':
        return {{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}};
    case 'L':
        return {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}};
    case 'M':
        return {{0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}};
    case 'N':
        return {{0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}};
    case 'O':
        return {{0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}};
    case 'P':
        return {{0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}};
    case 'Q':
        return {{0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}};
    case 'R':
        return {{0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}};
    case 'S':
        return {{0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}};
    case 'T':
        return {{0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}};
    case 'U':
        return {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}};
    case 'V':
        return {{0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}};
    case 'W':
        return {{0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11}};
    case 'X':
        return {{0x11, 0x0a, 0x04, 0x04, 0x04, 0x0a, 0x11}};
    case 'Y':
        return {{0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x04}};
    case 'Z':
        return {{0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}};
    case 'a':
        return {{0x00, 0x00, 0x0e, 0x01, 0x0f, 0x11, 0x0f}};
    case 'b':
        return {{0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x1e}};
    case 'c':
        return {{0x00, 0x00, 0x0e, 0x10, 0x10, 0x10, 0x0e}};
    case 'd':
        return {{0x01, 0x01, 0x0d, 0x13, 0x11, 0x11, 0x0f}};
    case 'e':
        return {{0x00, 0x00, 0x0e, 0x11, 0x1f, 0x10, 0x0e}};
    case 'f':
        return {{0x06, 0x08, 0x08, 0x1c, 0x08, 0x08, 0x08}};
    case 'g':
        return {{0x00, 0x00, 0x0f, 0x11, 0x0f, 0x01, 0x0e}};
    case 'h':
        return {{0x10, 0x10, 0x16, 0x19, 0x11, 0x11, 0x11}};
    case 'i':
        return {{0x04, 0x00, 0x0c, 0x04, 0x04, 0x04, 0x0e}};
    case 'j':
        return {{0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0c}};
    case 'k':
        return {{0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12}};
    case 'l':
        return {{0x0c, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}};
    case 'm':
        return {{0x00, 0x00, 0x1a, 0x15, 0x15, 0x15, 0x15}};
    case 'n':
        return {{0x00, 0x00, 0x16, 0x19, 0x11, 0x11, 0x11}};
    case 'o':
        return {{0x00, 0x00, 0x0e, 0x11, 0x11, 0x11, 0x0e}};
    case 'p':
        return {{0x00, 0x00, 0x1e, 0x11, 0x1e, 0x10, 0x10}};
    case 'q':
        return {{0x00, 0x00, 0x0f, 0x11, 0x0f, 0x01, 0x01}};
    case 'r':
        return {{0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10}};
    case 's':
        return {{0x00, 0x00, 0x0f, 0x10, 0x0e, 0x01, 0x1e}};
    case 't':
        return {{0x08, 0x08, 0x1c, 0x08, 0x08, 0x09, 0x06}};
    case 'u':
        return {{0x00, 0x00, 0x11, 0x11, 0x11, 0x13, 0x0d}};
    case 'v':
        return {{0x00, 0x00, 0x11, 0x11, 0x11, 0x0a, 0x04}};
    case 'w':
        return {{0x00, 0x00, 0x11, 0x15, 0x15, 0x15, 0x0a}};
    case 'x':
        return {{0x00, 0x00, 0x11, 0x0a, 0x04, 0x0a, 0x11}};
    case 'y':
        return {{0x00, 0x00, 0x11, 0x11, 0x0f, 0x01, 0x0e}};
    case 'z':
        return {{0x00, 0x00, 0x1f, 0x02, 0x04, 0x08, 0x1f}};
    case ':':
        return {{0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}};
    case '.':
        return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c}};
    case ',':
        return {{0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08}};
    case '-':
        return {{0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00}};
    case '_':
        return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f}};
    case '/':
        return {{0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10}};
    case '+':
        return {{0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00}};
    case '=':
        return {{0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00}};
    case '>':
        return {{0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10}};
    case '<':
        return {{0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01}};
    case '?':
        return {{0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}};
    case '!':
        return {{0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}};
    case '[':
        return {{0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e}};
    case ']':
        return {{0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e}};
    case ' ':
        return {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    default:
        return {{0x1f, 0x11, 0x15, 0x15, 0x15, 0x11, 0x1f}};
    }
}

void fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color) {
    g_state.surface.fill_rect({x, y, width, height}, {color});
}

void clear_cell(uint64_t column, uint64_t row) {
    fill_rect(column * kCellWidth, row * kCellHeight, kCellWidth, kCellHeight, g_state.background);
}

void draw_cursor(uint64_t column, uint64_t row, uint32_t color) {
    if (column >= g_state.console.columns() || row >= g_state.console.rows()) {
        return;
    }

    fill_rect(column * kCellWidth, (row * kCellHeight) + kCursorTop, kGlyphWidth * kGlyphScale,
              kCursorHeight, color);
}

void draw_glyph(char value, uint64_t column, uint64_t row) {
    clear_cell(column, row);

    const Glyph glyph = glyph_for(value);
    const uint64_t origin_x = column * kCellWidth;
    const uint64_t origin_y = row * kCellHeight;

    for (uint64_t glyph_row = 0; glyph_row < kGlyphHeight; ++glyph_row) {
        for (uint64_t glyph_column = 0; glyph_column < kGlyphWidth; ++glyph_column) {
            const uint8_t mask = static_cast<uint8_t>(1u << (kGlyphWidth - glyph_column - 1));
            if ((glyph.rows[glyph_row] & mask) == 0) {
                continue;
            }

            for (uint64_t dy = 0; dy < kGlyphScale; ++dy) {
                for (uint64_t dx = 0; dx < kGlyphScale; ++dx) {
                    g_state.surface.put_pixel(origin_x + (glyph_column * kGlyphScale) + dx,
                                              origin_y + (glyph_row * kGlyphScale) + dy,
                                              {g_state.foreground});
                }
            }
        }
    }
}

void scroll() { g_state.surface.scroll_up(kCellHeight, {g_state.background}); }

void hide_text_cursor() {
    if (!g_state.cursor_visible) {
        return;
    }

    draw_cursor(g_state.visible_cursor_column, g_state.visible_cursor_row, g_state.background);
    g_state.cursor_visible = false;
}

void apply_console_update(kernel::TextConsoleUpdate update) {
    switch (update.action) {
    case kernel::TextConsoleAction::DrawGlyph:
        draw_glyph(update.glyph, update.cell.column, update.cell.row);
        break;
    case kernel::TextConsoleAction::ClearCell:
        clear_cell(update.cell.column, update.cell.row);
        break;
    case kernel::TextConsoleAction::None:
        break;
    }

    if (update.scroll) {
        scroll();
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
    g_state.foreground = pack_rgb(*framebuffer, 0xf5, 0xf5, 0xf5);
    g_state.background = pack_rgb(*framebuffer, 0x10, 0x14, 0x1c);

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
    kernel::mouse_cursor::hide();
    fill_rect(0, 0, g_state.surface.width(), g_state.surface.height(), g_state.background);
    g_state.console.clear();
    kernel::mouse_cursor::show();
}

void clear_row_from(uint64_t column, uint64_t row) {
    if (!ready() || row >= g_state.console.rows()) {
        return;
    }

    kernel::mouse_cursor::hide();
    while (column < g_state.console.columns()) {
        clear_cell(column, row);
        ++column;
    }
    kernel::mouse_cursor::show();
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

    kernel::mouse_cursor::hide();
    hide_text_cursor();
    draw_cursor(g_state.console.cursor_column(), g_state.console.cursor_row(), g_state.foreground);
    g_state.visible_cursor_column = g_state.console.cursor_column();
    g_state.visible_cursor_row = g_state.console.cursor_row();
    g_state.cursor_visible = true;
    kernel::mouse_cursor::show();
}

void hide_cursor() {
    if (!ready() || !g_state.cursor_visible) {
        return;
    }

    kernel::mouse_cursor::hide();
    hide_text_cursor();
    kernel::mouse_cursor::show();
}

void write_char(char value) {
    if (!ready()) {
        return;
    }

    kernel::mouse_cursor::hide();
    switch (value) {
    case '\n':
        apply_console_update(g_state.console.newline());
        kernel::mouse_cursor::show();
        return;
    case '\r':
        g_state.console.carriage_return();
        kernel::mouse_cursor::show();
        return;
    case '\t':
        kernel::mouse_cursor::show();
        for (int index = 0; index < 4; ++index) {
            write_char(' ');
        }
        return;
    case '\b':
        apply_console_update(g_state.console.backspace());
        kernel::mouse_cursor::show();
        return;
    default:
        break;
    }

    apply_console_update(g_state.console.write_char(value));
    kernel::mouse_cursor::show();
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

} // namespace kernel::terminal
