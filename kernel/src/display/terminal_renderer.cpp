#include "kernel/display/terminal_renderer.hpp"

namespace kernel::display {

void TerminalRenderer::reset(Surface& surface, Color foreground, Color background) {
    surface_ = &surface;
    foreground_ = foreground;
    background_ = background;
}

void TerminalRenderer::fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height,
                                 Color color) {
    if (surface_ == nullptr) {
        return;
    }

    surface_->fill_rect({x, y, width, height}, color);
}

void TerminalRenderer::clear_screen() {
    if (!ready()) {
        return;
    }

    surface_->fill_rect({0, 0, surface_->width(), surface_->height()}, background_);
}

void TerminalRenderer::clear_cell(uint64_t column, uint64_t row) {
    fill_rect(column * kCellWidth, row * kCellHeight, kCellWidth, kCellHeight, background_);
}

void TerminalRenderer::draw_glyph(char value, uint64_t column, uint64_t row) {
    if (!ready()) {
        return;
    }

    clear_cell(column, row);

    const Glyph5x7& glyph = Font5x7::glyph_for(value);
    const uint64_t origin_x = column * kCellWidth;
    const uint64_t origin_y = row * kCellHeight;

    for (uint64_t glyph_row = 0; glyph_row < Glyph5x7::height; ++glyph_row) {
        for (uint64_t glyph_column = 0; glyph_column < Glyph5x7::width; ++glyph_column) {
            const uint8_t mask = static_cast<uint8_t>(1u << (Glyph5x7::width - glyph_column - 1));
            if ((glyph.rows[glyph_row] & mask) == 0) {
                continue;
            }

            for (uint64_t dy = 0; dy < kGlyphScale; ++dy) {
                for (uint64_t dx = 0; dx < kGlyphScale; ++dx) {
                    surface_->put_pixel(origin_x + (glyph_column * kGlyphScale) + dx,
                                        origin_y + (glyph_row * kGlyphScale) + dy, foreground_);
                }
            }
        }
    }
}

void TerminalRenderer::draw_cursor(uint64_t column, uint64_t row) {
    fill_rect(column * kCellWidth, (row * kCellHeight) + kCursorTop, Glyph5x7::width * kGlyphScale,
              kCursorHeight, foreground_);
}

void TerminalRenderer::erase_cursor(uint64_t column, uint64_t row) {
    fill_rect(column * kCellWidth, (row * kCellHeight) + kCursorTop, Glyph5x7::width * kGlyphScale,
              kCursorHeight, background_);
}

void TerminalRenderer::scroll() {
    if (!ready()) {
        return;
    }

    surface_->scroll_up(kCellHeight, background_);
}

} // namespace kernel::display
