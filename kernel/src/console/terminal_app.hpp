#pragma once

#include <stdint.h>

#include "kernel/base/string_view.hpp"
#include "kernel/display/app_surface.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/terminal_render_cache.hpp"
#include "kernel/display/terminal_renderer.hpp"
#include "kernel/display/terminal_repaint_state.hpp"
#include "kernel/text/text_buffer.hpp"
#include "kernel/text/text_console.hpp"

namespace kernel::console
{

struct TerminalRepaintSink
{
    void (*repaint_layers_above)(display::Rect rect) = nullptr;

    bool ready() const { return repaint_layers_above != nullptr; }
};

struct TerminalCursorState
{
    uint64_t column = 0;
    uint64_t row = 0;
    bool visible = false;

    void reset()
    {
        column = 0;
        row = 0;
        visible = false;
    }

    void show(uint64_t next_column, uint64_t next_row)
    {
        column = next_column;
        row = next_row;
        visible = true;
    }

    void hide() { visible = false; }
};

class TerminalApp
{
public:
    static constexpr uint64_t kCellWidth = display::TerminalRenderer::kCellWidth;
    static constexpr uint64_t kCellHeight = display::TerminalRenderer::kCellHeight;

    TerminalApp() = default;

    bool reset(display::Surface & surface,
               display::AppSurface app_surface,
               display::Color foreground,
               display::Color background,
               TerminalRepaintSink repaint_sink);

    bool ready() const;
    display::Rect bounds() const { return app_surface_.bounds; }
    uint64_t columns() const { return console_.columns(); }
    uint64_t rows() const { return console_.rows(); }
    uint64_t cursor_column() const { return console_.cursor_column(); }
    uint64_t cursor_row() const { return console_.cursor_row(); }

    void begin_update();
    void end_update();
    void repaint_region(display::Rect dirty_rect);

    void clear();
    void clear_cell_at(uint64_t column, uint64_t row);
    void clear_row_from(uint64_t column, uint64_t row);
    void draw_char_at(uint64_t column, uint64_t row, char value);
    void set_cursor(uint64_t column, uint64_t row);
    void show_cursor();
    void hide_cursor();
    void write_char(char value);
    void write_string(StringView value);
    void write_string(const char * value);
    void write_line(StringView value);
    void write_line(const char * value);
    void write_hex(uint64_t value);
    void write_decimal(uint64_t value);

private:
    uint64_t text_grid_width() const;
    uint64_t text_grid_height() const;
    display::Rect cell_rect(uint64_t column, uint64_t row) const;
    display::Rect row_tail_rect(uint64_t column, uint64_t row) const;

    void hide_text_cursor();
    void repaint_text_layer();
    void render_text_cell(uint64_t column, uint64_t row, char glyph);
    void render_buffer_cell(uint64_t column, uint64_t row);
    void clear_gutter_region(display::Rect gutter, display::Rect dirty_rect);
    void clear_terminal_gutters(display::Rect dirty_rect);
    display::Rect render_dirty_text_cells();
    display::Rect render_text_repaint(bool repaint_entire_layer);
    void repaint_layers_above(display::Rect dirty_rect);
    void apply_repaint(display::Rect dirty_rect,
                       bool repaint_text_layer,
                       bool repaint_entire_text_layer,
                       bool repaint_higher_layers);
    void apply_repaint_request(display::TerminalRepaintRequest request);
    void apply_repaint_flush(display::TerminalRepaintFlush flush);
    void record_console_dirty(display::Rect dirty_rect);
    display::Rect apply_console_update(text::TextConsoleUpdate update);
    display::Rect apply_write_character(char value);
    void write_tab();

    display::AppSurface app_surface_;
    display::TerminalRenderer renderer_;
    TerminalRepaintSink repaint_sink_;
    text::TextConsole console_;
    text::TextBuffer text_buffer_;
    display::TerminalRenderCache render_cache_;
    display::TerminalRepaintState repaint_;
    TerminalCursorState cursor_;
};

} // namespace kernel::console
