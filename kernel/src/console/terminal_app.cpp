#include "terminal_app.hpp"

#include <stddef.h>

namespace kernel::console
{

bool TerminalApp::reset(display::Surface & surface,
                        display::AppSurface app_surface,
                        display::Color foreground,
                        display::Color background,
                        TerminalRepaintSink repaint_sink)
{
    app_surface_ = app_surface;
    if (!surface.ready() || !app_surface_.valid() || !repaint_sink.ready())
    {
        return false;
    }

    const uint64_t column_count = app_surface_.bounds.width / kCellWidth;
    const uint64_t row_count = app_surface_.bounds.height / kCellHeight;
    if (column_count == 0 || row_count == 0 || !text_buffer_.reset(column_count, row_count) ||
        !render_cache_.reset(column_count, row_count))
    {
        app_surface_ = {};
        return false;
    }

    console_.reset(column_count, row_count);
    renderer_.reset(surface, app_surface_.bounds, foreground, background);
    repaint_sink_ = repaint_sink;
    repaint_.reset();
    visible_cursor_column_ = 0;
    visible_cursor_row_ = 0;
    cursor_visible_ = false;
    return renderer_.ready();
}

bool TerminalApp::ready() const
{
    return renderer_.ready() && app_surface_.valid();
}

void TerminalApp::begin_update()
{
    if (!ready())
    {
        return;
    }

    repaint_.begin_batch();
}

void TerminalApp::end_update()
{
    if (!ready())
    {
        return;
    }

    const display::TerminalRepaintFlush flush = repaint_.end_batch();
    if (flush.outermost_batch_ended)
    {
        apply_repaint_flush(flush);
    }
}

uint64_t TerminalApp::text_grid_width() const
{
    return text_buffer_.columns() * kCellWidth;
}

uint64_t TerminalApp::text_grid_height() const
{
    return text_buffer_.rows() * kCellHeight;
}

display::Rect TerminalApp::cell_rect(uint64_t column, uint64_t row) const
{
    return {
        app_surface_.bounds.x + (column * kCellWidth),
        app_surface_.bounds.y + (row * kCellHeight),
        kCellWidth,
        kCellHeight,
    };
}

display::Rect TerminalApp::row_tail_rect(uint64_t column, uint64_t row) const
{
    if (column >= console_.columns())
    {
        return {};
    }

    return {
        app_surface_.bounds.x + (column * kCellWidth),
        app_surface_.bounds.y + (row * kCellHeight),
        (console_.columns() - column) * kCellWidth,
        kCellHeight,
    };
}

void TerminalApp::hide_text_cursor()
{
    if (!cursor_visible_)
    {
        return;
    }

    renderer_.erase_cursor(visible_cursor_column_, visible_cursor_row_);
    cursor_visible_ = false;
}

void TerminalApp::repaint_text_layer()
{
    renderer_.clear_screen();
    for (uint64_t row = 0; row < text_buffer_.rows(); ++row)
    {
        for (uint64_t column = 0; column < text_buffer_.columns(); ++column)
        {
            const char glyph = text_buffer_.glyph_at(column, row);
            if (glyph != text::kTextBufferBlank)
            {
                renderer_.draw_glyph(glyph, column, row);
            }
        }
    }
    render_cache_.synchronize_from(text_buffer_);
}

void TerminalApp::render_text_cell(uint64_t column, uint64_t row, char glyph)
{
    if (glyph == text::kTextBufferBlank)
    {
        renderer_.clear_cell(column, row);
    }
    else
    {
        renderer_.draw_glyph(glyph, column, row);
    }
    (void)render_cache_.mark_rendered(column, row, glyph);
}

void TerminalApp::render_buffer_cell(uint64_t column, uint64_t row)
{
    render_text_cell(column, row, text_buffer_.glyph_at(column, row));
}

void TerminalApp::clear_gutter_region(display::Rect gutter, display::Rect dirty_rect)
{
    const display::Rect clipped = display::intersect_rect(gutter, dirty_rect);
    if (!clipped.empty())
    {
        renderer_.clear_rect(clipped);
    }
}

void TerminalApp::clear_terminal_gutters(display::Rect dirty_rect)
{
    const uint64_t grid_width = text_grid_width();
    const uint64_t grid_height = text_grid_height();
    const display::Rect terminal_bounds = bounds();

    if (grid_width < terminal_bounds.width)
    {
        clear_gutter_region({
                                terminal_bounds.x + grid_width,
                                terminal_bounds.y,
                                terminal_bounds.width - grid_width,
                                terminal_bounds.height,
                            },
                            dirty_rect);
    }
    if (grid_height < terminal_bounds.height)
    {
        clear_gutter_region({
                                terminal_bounds.x,
                                terminal_bounds.y + grid_height,
                                terminal_bounds.width,
                                terminal_bounds.height - grid_height,
                            },
                            dirty_rect);
    }
}

display::Rect TerminalApp::render_dirty_text_cells()
{
    display::Rect dirty_rect;
    for (uint64_t row = 0; row < text_buffer_.rows(); ++row)
    {
        for (uint64_t column = 0; column < text_buffer_.columns(); ++column)
        {
            if (!render_cache_.needs_render(text_buffer_, column, row))
            {
                continue;
            }

            render_buffer_cell(column, row);
            dirty_rect = display::bounding_rect(dirty_rect, cell_rect(column, row));
        }
    }
    return dirty_rect;
}

display::Rect TerminalApp::render_text_repaint(bool full_repaint, uint64_t scroll_rows)
{
    (void)scroll_rows;
    if (full_repaint || !render_cache_.valid())
    {
        repaint_text_layer();
        return bounds();
    }

    return render_dirty_text_cells();
}

void TerminalApp::repaint_layers_above(display::Rect dirty_rect)
{
    if (!dirty_rect.empty() && repaint_sink_.repaint_layers_above != nullptr)
    {
        repaint_sink_.repaint_layers_above(dirty_rect);
    }
}

void TerminalApp::repaint_region(display::Rect dirty_rect)
{
    if (!ready() || dirty_rect.empty())
    {
        return;
    }

    clear_terminal_gutters(dirty_rect);

    for (uint64_t row = 0; row < text_buffer_.rows(); ++row)
    {
        for (uint64_t column = 0; column < text_buffer_.columns(); ++column)
        {
            if (display::rects_overlap(cell_rect(column, row), dirty_rect))
            {
                render_buffer_cell(column, row);
            }
        }
    }

    if (cursor_visible_ &&
        display::rects_overlap(cell_rect(visible_cursor_column_, visible_cursor_row_), dirty_rect))
    {
        renderer_.draw_cursor(visible_cursor_column_, visible_cursor_row_);
    }
}

void TerminalApp::apply_repaint_request(display::TerminalRepaintRequest request)
{
    display::Rect dirty_rect = request.dirty_rect;
    if (request.repaint_text_layer)
    {
        dirty_rect = display::bounding_rect(dirty_rect,
                                            render_text_repaint(request.full_text_repaint,
                                                                request.scroll_rows));
    }

    if (request.repaint_higher_layers)
    {
        repaint_layers_above(dirty_rect);
    }
}

void TerminalApp::apply_repaint_flush(display::TerminalRepaintFlush flush)
{
    display::Rect dirty_rect = flush.dirty_rect;
    if (flush.repaint_text_layer)
    {
        dirty_rect = display::bounding_rect(dirty_rect,
                                            render_text_repaint(flush.full_text_repaint,
                                                                flush.scroll_rows));
    }

    if (flush.repaint_higher_layers)
    {
        repaint_layers_above(dirty_rect);
    }
}

void TerminalApp::record_console_dirty(display::Rect dirty_rect)
{
    apply_repaint_request(repaint_.record_dirty(dirty_rect));
}

display::Rect TerminalApp::apply_console_update(text::TextConsoleUpdate update)
{
    display::Rect dirty_rect;
    const bool draw_immediately =
        !update.scroll && !repaint_.pending_text_repaint() && render_cache_.valid();

    switch (update.action)
    {
    case text::TextConsoleAction::DrawGlyph:
        text_buffer_.put(update.cell.column, update.cell.row, update.glyph);
        if (draw_immediately)
        {
            render_text_cell(update.cell.column, update.cell.row, update.glyph);
            dirty_rect = cell_rect(update.cell.column, update.cell.row);
        }
        break;
    case text::TextConsoleAction::ClearCell:
        text_buffer_.clear_cell(update.cell.column, update.cell.row);
        if (draw_immediately)
        {
            render_text_cell(update.cell.column, update.cell.row, text::kTextBufferBlank);
            dirty_rect = cell_rect(update.cell.column, update.cell.row);
        }
        break;
    case text::TextConsoleAction::None:
        break;
    }

    if (update.scroll)
    {
        text_buffer_.scroll_up();
        apply_repaint_request(repaint_.record_scroll(bounds(), text_buffer_.rows()));
        return {};
    }

    return dirty_rect;
}

void TerminalApp::clear()
{
    if (!ready())
    {
        return;
    }

    cursor_visible_ = false;
    renderer_.clear_screen();
    console_.clear();
    text_buffer_.clear();
    render_cache_.clear_rendered();
    record_console_dirty(bounds());
}

void TerminalApp::clear_cell_at(uint64_t column, uint64_t row)
{
    if (!ready() || column >= console_.columns() || row >= console_.rows())
    {
        return;
    }

    text_buffer_.clear_cell(column, row);
    render_text_cell(column, row, text::kTextBufferBlank);
    record_console_dirty(cell_rect(column, row));
}

void TerminalApp::clear_row_from(uint64_t column, uint64_t row)
{
    if (!ready() || row >= console_.rows())
    {
        return;
    }

    const display::Rect dirty_rect = row_tail_rect(column, row);
    while (column < console_.columns())
    {
        text_buffer_.clear_cell(column, row);
        render_text_cell(column, row, text::kTextBufferBlank);
        ++column;
    }
    record_console_dirty(dirty_rect);
}

void TerminalApp::draw_char_at(uint64_t column, uint64_t row, char value)
{
    if (!ready() || column >= console_.columns() || row >= console_.rows())
    {
        return;
    }

    text_buffer_.put(column, row, value);
    render_text_cell(column, row, value);
    record_console_dirty(cell_rect(column, row));
}

void TerminalApp::set_cursor(uint64_t column, uint64_t row)
{
    if (ready())
    {
        console_.set_cursor(column, row);
    }
}

void TerminalApp::show_cursor()
{
    if (!ready())
    {
        return;
    }

    hide_text_cursor();
    renderer_.draw_cursor(console_.cursor_column(), console_.cursor_row());
    visible_cursor_column_ = console_.cursor_column();
    visible_cursor_row_ = console_.cursor_row();
    cursor_visible_ = true;
    record_console_dirty(cell_rect(console_.cursor_column(), console_.cursor_row()));
}

void TerminalApp::hide_cursor()
{
    if (!ready() || !cursor_visible_)
    {
        return;
    }

    hide_text_cursor();
    record_console_dirty(cell_rect(visible_cursor_column_, visible_cursor_row_));
}

void TerminalApp::write_char(char value)
{
    if (!ready())
    {
        return;
    }

    if (value == '\t')
    {
        for (int index = 0; index < 4; ++index)
        {
            write_char(' ');
        }
        return;
    }

    display::Rect dirty_rect;
    switch (value)
    {
    case '\n':
        dirty_rect = apply_console_update(console_.newline());
        break;
    case '\r':
        console_.carriage_return();
        break;
    case '\b':
        dirty_rect = apply_console_update(console_.backspace());
        break;
    default:
        dirty_rect = apply_console_update(console_.write_char(value));
        break;
    }
    record_console_dirty(dirty_rect);
}

void TerminalApp::write_string(StringView value)
{
    for (char character : value)
    {
        write_char(character);
    }
}

void TerminalApp::write_string(const char * value)
{
    write_string(StringView(value));
}

void TerminalApp::write_line(StringView value)
{
    write_string(value);
    write_char('\n');
}

void TerminalApp::write_line(const char * value)
{
    write_line(StringView(value));
}

void TerminalApp::write_hex(uint64_t value)
{
    static constexpr char digits[] = "0123456789abcdef";
    write_string("0x");

    for (int shift = 60; shift >= 0; shift -= 4)
    {
        const auto nibble = static_cast<uint8_t>((value >> shift) & 0xf);
        write_char(digits[nibble]);
    }
}

void TerminalApp::write_decimal(uint64_t value)
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

} // namespace kernel::console
