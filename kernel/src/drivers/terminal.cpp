#include "kernel/terminal.hpp"

#include <stdint.h>

#include "kernel/limine_support.hpp"

namespace {

constexpr uint64_t kGlyphWidth = 5;
constexpr uint64_t kGlyphHeight = 7;
constexpr uint64_t kGlyphScale = 2;
constexpr uint64_t kCellWidth = (kGlyphWidth + 1) * kGlyphScale;
constexpr uint64_t kCellHeight = (kGlyphHeight + 2) * kGlyphScale;

struct Glyph {
    uint8_t rows[kGlyphHeight];
};

struct TerminalState {
    limine_framebuffer* framebuffer = nullptr;
    uint64_t columns = 0;
    uint64_t rows = 0;
    uint64_t cursor_column = 0;
    uint64_t cursor_row = 0;
    uint32_t foreground = 0;
    uint32_t background = 0;
};

TerminalState g_state;

uint32_t pack_rgb(const limine_framebuffer& framebuffer, uint8_t red, uint8_t green, uint8_t blue) {
    return (static_cast<uint32_t>(red) << framebuffer.red_mask_shift) |
           (static_cast<uint32_t>(green) << framebuffer.green_mask_shift) |
           (static_cast<uint32_t>(blue) << framebuffer.blue_mask_shift);
}

char normalize(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }

    return value;
}

Glyph glyph_for(char value) {
    switch (normalize(value)) {
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
    case 'a':
        return {{0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}};
    case 'b':
        return {{0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}};
    case 'c':
        return {{0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f}};
    case 'd':
        return {{0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}};
    case 'e':
        return {{0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}};
    case 'f':
        return {{0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}};
    case 'g':
        return {{0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f}};
    case 'h':
        return {{0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}};
    case 'i':
        return {{0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}};
    case 'j':
        return {{0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e}};
    case 'k':
        return {{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}};
    case 'l':
        return {{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}};
    case 'm':
        return {{0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}};
    case 'n':
        return {{0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}};
    case 'o':
        return {{0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}};
    case 'p':
        return {{0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}};
    case 'q':
        return {{0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}};
    case 'r':
        return {{0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}};
    case 's':
        return {{0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}};
    case 't':
        return {{0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}};
    case 'u':
        return {{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}};
    case 'v':
        return {{0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}};
    case 'w':
        return {{0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11}};
    case 'x':
        return {{0x11, 0x0a, 0x04, 0x04, 0x04, 0x0a, 0x11}};
    case 'y':
        return {{0x11, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x04}};
    case 'z':
        return {{0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}};
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

void put_pixel(uint64_t x, uint64_t y, uint32_t color) {
    auto* base = static_cast<uint8_t*>(g_state.framebuffer->address);
    auto* pixel = reinterpret_cast<uint32_t*>(base + (y * g_state.framebuffer->pitch) +
                                              (x * sizeof(uint32_t)));
    *pixel = color;
}

void fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t color) {
    for (uint64_t row = 0; row < height; ++row) {
        auto* base = static_cast<uint8_t*>(g_state.framebuffer->address);
        auto* pixel = reinterpret_cast<uint32_t*>(base + ((y + row) * g_state.framebuffer->pitch) +
                                                  (x * sizeof(uint32_t)));
        for (uint64_t column = 0; column < width; ++column) {
            pixel[column] = color;
        }
    }
}

void clear_cell(uint64_t column, uint64_t row) {
    fill_rect(column * kCellWidth, row * kCellHeight, kCellWidth, kCellHeight, g_state.background);
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
                    put_pixel(origin_x + (glyph_column * kGlyphScale) + dx,
                              origin_y + (glyph_row * kGlyphScale) + dy, g_state.foreground);
                }
            }
        }
    }
}

void clear_rows(uint64_t start_pixel_y, uint64_t height) {
    fill_rect(0, start_pixel_y, g_state.framebuffer->width, height, g_state.background);
}

void scroll() {
    auto* base = static_cast<uint8_t*>(g_state.framebuffer->address);
    const uint64_t copy_height = g_state.framebuffer->height - kCellHeight;

    for (uint64_t y = 0; y < copy_height; ++y) {
        auto* destination = base + (y * g_state.framebuffer->pitch);
        const auto* source = base + ((y + kCellHeight) * g_state.framebuffer->pitch);
        for (uint64_t byte = 0; byte < g_state.framebuffer->pitch; ++byte) {
            destination[byte] = source[byte];
        }
    }

    clear_rows(copy_height, kCellHeight);
}

void newline() {
    g_state.cursor_column = 0;
    if (g_state.cursor_row + 1 >= g_state.rows) {
        scroll();
        return;
    }

    ++g_state.cursor_row;
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

    g_state.framebuffer = framebuffer;
    g_state.columns = framebuffer->width / kCellWidth;
    g_state.rows = framebuffer->height / kCellHeight;
    g_state.cursor_column = 0;
    g_state.cursor_row = 0;
    g_state.foreground = pack_rgb(*framebuffer, 0xf5, 0xf5, 0xf5);
    g_state.background = pack_rgb(*framebuffer, 0x10, 0x14, 0x1c);

    clear();
    return true;
}

bool ready() { return g_state.framebuffer != nullptr; }

void clear() {
    if (!ready()) {
        return;
    }

    fill_rect(0, 0, g_state.framebuffer->width, g_state.framebuffer->height, g_state.background);
    g_state.cursor_column = 0;
    g_state.cursor_row = 0;
}

void write_char(char value) {
    if (!ready()) {
        return;
    }

    switch (value) {
    case '\n':
        newline();
        return;
    case '\r':
        g_state.cursor_column = 0;
        return;
    case '\t':
        for (int index = 0; index < 4; ++index) {
            write_char(' ');
        }
        return;
    case '\b':
        if (g_state.cursor_column > 0) {
            --g_state.cursor_column;
            clear_cell(g_state.cursor_column, g_state.cursor_row);
        }
        return;
    default:
        break;
    }

    draw_glyph(value, g_state.cursor_column, g_state.cursor_row);

    ++g_state.cursor_column;
    if (g_state.cursor_column >= g_state.columns) {
        newline();
    }
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
