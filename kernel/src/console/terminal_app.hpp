#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/base/string_view.hpp"
#include "kernel/display/app_layout.hpp"
#include "kernel/display/app_surface.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/scroll_mapped_surface.hpp"
#include "kernel/display/terminal_render_cache.hpp"
#include "kernel/display/terminal_renderer.hpp"
#include "kernel/display/terminal_repaint_state.hpp"
#include "kernel/text/text_buffer.hpp"
#include "kernel/text/text_console.hpp"

namespace kernel::console
{

struct TerminalRepaintSink
{
    void (*submit_app_surface_damage)(display::FrameDamage damage) = nullptr;

    bool ready() const { return submit_app_surface_damage != nullptr; }
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

enum class TerminalResizePolicy
{
    Clear,
};

class TerminalApp
{
public:
    static constexpr uint64_t kCellWidth = display::TerminalRenderer::kCellWidth;
    static constexpr uint64_t kCellHeight = display::TerminalRenderer::kCellHeight;

    TerminalApp() = default;

    bool reset(display::AppSurface app_surface,
               display::Color foreground,
               display::Color background,
               TerminalRepaintSink repaint_sink);
    bool resize(display::AppSurface app_surface,
                TerminalResizePolicy policy = TerminalResizePolicy::Clear);

    bool ready() const;
    display::Rect bounds() const { return app_surface_.bounds; }
    uint64_t columns() const { return console_.columns(); }
    uint64_t rows() const { return console_.rows(); }
    uint64_t cursor_column() const { return console_.cursor_column(); }
    uint64_t cursor_row() const { return console_.cursor_row(); }

    void begin_update();
    void end_update();

    display::PixelSample sample_pixel(uint64_t x, uint64_t y) const;
    display::PixelSample sample_caret_pixel(uint64_t x, uint64_t y) const;
    const uint32_t * row_pixels(uint64_t y) const;
    display::Rect caret_bounds() const;

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
    display::Rect text_grid_rect() const;
    display::Rect cell_rect(uint64_t column, uint64_t row) const;
    display::Rect row_tail_rect(uint64_t column, uint64_t row) const;

    display::Rect caret_rect(uint64_t column, uint64_t row) const;
    void repaint_text_layer();
    void render_text_cell(uint64_t column, uint64_t row, char glyph);
    void render_buffer_cell(uint64_t column, uint64_t row);
    display::Rect scroll_backing_text_grid_up(uint64_t rows);
    display::Rect render_text_cells_in_rect(display::Rect rect);
    display::Rect render_dirty_text_cells();
    display::Rect render_text_repaint(bool repaint_entire_layer);
    bool allocate_backing_surface_for(display::AppSurface app_surface,
                                      uint32_t *& memory,
                                      size_t & bytes,
                                      display::BackingSurface & backing_storage,
                                      display::ScrollMappedSurface & backing) const;
    bool replace_surface(display::AppSurface app_surface);
    bool update_scope_active() const { return update_depth_ > 0; }
    bool pending_backing_scroll() const { return pending_scroll_rows_ > 0; }
    void record_pending_dirty(display::Rect rect);
    void record_pending_scroll();
    void flush_pending_backing_scroll();
    void apply_repaint(display::FrameDamage damage);
    void apply_repaint_request(display::TerminalRepaintRequest request);
    void record_console_dirty(display::Rect dirty_rect);
    display::Rect apply_console_update(text::TextConsoleUpdate update);
    display::Rect apply_write_character(char value);
    void write_tab();

    display::AppSurface app_surface_;
    display::BackingSurface backing_storage_;
    display::ScrollMappedSurface backing_;
    display::TerminalRenderer renderer_;
    TerminalRepaintSink repaint_sink_;
    display::Color foreground_;
    display::Color background_;
    text::TextConsole console_;
    text::TextBuffer text_buffer_;
    display::TerminalRenderCache render_cache_;
    display::TerminalRepaintState repaint_;
    TerminalCursorState cursor_;
    uint32_t * backing_memory_ = nullptr;
    size_t backing_bytes_ = 0;
    uint32_t update_depth_ = 0;
    uint64_t pending_scroll_rows_ = 0;
    display::Rect pending_dirty_after_scroll_;
};

} // namespace kernel::console
