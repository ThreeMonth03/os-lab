#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/text/font5x7.hpp"

namespace kernel::display
{

class TerminalRenderer
{
public:
    static constexpr uint64_t kGlyphScale = 2;
    static constexpr uint64_t kCellWidth = (text::Glyph5x7::width + 1) * kGlyphScale;
    static constexpr uint64_t kCellHeight = (text::Glyph5x7::height + 2) * kGlyphScale;

    TerminalRenderer() = default;

    void reset(Surface & surface, Rect viewport, Color foreground, Color background);

    bool ready() const { return surface_ != nullptr && surface_->ready() && !viewport_.empty(); }
    Rect viewport() const { return viewport_; }

    void clear_screen();
    void clear_rect(Rect rect);
    void clear_cell(uint64_t column, uint64_t row);
    void scroll_up_rows(uint64_t rows);
    void draw_glyph(char value, uint64_t column, uint64_t row);
    void draw_cursor(uint64_t column, uint64_t row);
    void erase_cursor(uint64_t column, uint64_t row);

private:
    static constexpr uint64_t kCursorTop = (text::Glyph5x7::height * kGlyphScale) + 1;
    static constexpr uint64_t kCursorHeight = kGlyphScale;

    void fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, Color color);

    Surface * surface_ = nullptr;
    Rect viewport_;
    Color foreground_;
    Color background_;
};

} // namespace kernel::display
