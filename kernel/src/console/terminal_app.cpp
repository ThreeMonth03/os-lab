#include "terminal_app.hpp"

#include <stddef.h>

#include "kernel/memory/heap.hpp"

namespace kernel::console
{

bool TerminalApp::reset(display::AppSurface app_surface,
                        display::Color foreground,
                        display::Color background,
                        TerminalRepaintSink repaint_sink)
{
    if (backing_memory_ != nullptr)
    {
        if (!kernel::memory::heap::free(backing_memory_))
        {
            return false;
        }
        backing_memory_ = nullptr;
        backing_bytes_ = 0;
        backing_storage_ = {};
        backing_ = {};
    }

    app_surface_ = app_surface;
    if (!app_surface_.valid() || !repaint_sink.ready())
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

    if (!allocate_backing_surface())
    {
        app_surface_ = {};
        return false;
    }

    console_.reset(column_count, row_count);
    renderer_.reset(backing_, app_surface_.bounds, foreground, background);
    repaint_sink_ = repaint_sink;
    repaint_.reset(app_surface_.bounds);
    cursor_.reset();
    update_depth_ = 0;
    pending_scroll_rows_ = 0;
    pending_dirty_after_scroll_ = {};
    return renderer_.ready();
}

bool TerminalApp::ready() const
{
    return renderer_.ready() && backing_.ready() && app_surface_.valid();
}

bool TerminalApp::allocate_backing_surface()
{
    size_t bytes = 0;
    if (!display::backing_surface_required_bytes(app_surface_.bounds, bytes))
    {
        return false;
    }

    void * memory = kernel::memory::heap::allocate(bytes, alignof(uint32_t));
    if (memory == nullptr)
    {
        return false;
    }

    backing_memory_ = static_cast<uint32_t *>(memory);
    backing_bytes_ = bytes;
    backing_storage_ =
        display::BackingSurface(backing_memory_, app_surface_.bounds, app_surface_.bounds.width);
    backing_.reset(backing_storage_, text_grid_rect());
    return backing_.ready();
}

display::PixelSample TerminalApp::sample_pixel(uint64_t x, uint64_t y) const
{
    return ready() ? backing_.sample(x, y) : display::transparent_pixel();
}

display::PixelSample TerminalApp::sample_caret_pixel(uint64_t x, uint64_t y) const
{
    return ready() && cursor_.visible && !display::intersect_rect(caret_bounds(), {x, y, 1, 1}).empty()
               ? display::opaque_pixel(renderer_.foreground())
               : display::transparent_pixel();
}

const uint32_t * TerminalApp::row_pixels(uint64_t y) const
{
    return ready() ? backing_.row_pixels(y) : nullptr;
}

uint64_t TerminalApp::text_grid_width() const
{
    return text_buffer_.columns() * kCellWidth;
}

uint64_t TerminalApp::text_grid_height() const
{
    return text_buffer_.rows() * kCellHeight;
}

display::Rect TerminalApp::text_grid_rect() const
{
    return {
        app_surface_.bounds.x,
        app_surface_.bounds.y,
        text_grid_width(),
        text_grid_height(),
    };
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

display::Rect TerminalApp::caret_rect(uint64_t column, uint64_t row) const
{
    static constexpr uint64_t kCursorTop =
        (text::Glyph5x7::height * display::TerminalRenderer::kGlyphScale) + 1;
    static constexpr uint64_t kCursorHeight = display::TerminalRenderer::kGlyphScale;

    return {
        app_surface_.bounds.x + (column * kCellWidth),
        app_surface_.bounds.y + (row * kCellHeight) + kCursorTop,
        text::Glyph5x7::width * display::TerminalRenderer::kGlyphScale,
        kCursorHeight,
    };
}

display::Rect TerminalApp::caret_bounds() const
{
    return ready() && cursor_.visible ? caret_rect(cursor_.column, cursor_.row) : display::Rect{};
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

void TerminalApp::begin_update()
{
    if (ready())
    {
        ++update_depth_;
    }
}

void TerminalApp::end_update()
{
    if (update_depth_ == 0)
    {
        return;
    }

    --update_depth_;
    if (update_depth_ == 0)
    {
        flush_pending_backing_scroll();
    }
}

void TerminalApp::record_pending_dirty(display::Rect rect)
{
    rect = display::intersect_rect(rect, text_grid_rect());
    if (!rect.empty())
    {
        pending_dirty_after_scroll_ = display::bounding_rect(pending_dirty_after_scroll_, rect);
    }
}

void TerminalApp::record_pending_scroll()
{
    if (pending_scroll_rows_ < text_buffer_.rows())
    {
        ++pending_scroll_rows_;
    }

    const uint64_t distance = pending_scroll_rows_ * kCellHeight;
    record_pending_dirty(display::exposed_scroll_region({text_grid_rect(), distance}));
}

void TerminalApp::flush_pending_backing_scroll()
{
    if (!pending_backing_scroll())
    {
        return;
    }

    const uint64_t rows = pending_scroll_rows_;
    const display::Rect pending_dirty = pending_dirty_after_scroll_;
    pending_scroll_rows_ = 0;
    pending_dirty_after_scroll_ = {};

    if (!render_cache_.valid() || rows >= text_buffer_.rows())
    {
        repaint_text_layer();
        apply_repaint_request(repaint_.record_dirty(bounds()));
        return;
    }

    const display::Rect scroll_dirty = scroll_backing_text_grid_up(rows);
    if (scroll_dirty.empty())
    {
        repaint_text_layer();
        apply_repaint_request(repaint_.record_dirty(bounds()));
        return;
    }

    render_cache_.scroll_up(rows);
    const uint64_t distance = rows * kCellHeight;
    const display::Rect exposed = display::exposed_scroll_region({text_grid_rect(), distance});
    const display::Rect rendered_dirty =
        render_text_cells_in_rect(display::bounding_rect(pending_dirty, exposed));

    display::FrameDamage damage;
    if (!damage.append_scroll({scroll_dirty, distance}) ||
        !damage.append_dirty(exposed, true) ||
        !damage.append_dirty(rendered_dirty))
    {
        apply_repaint_request(repaint_.record_dirty(bounds()));
        return;
    }

    if (repaint_sink_.submit_terminal_damage != nullptr)
    {
        repaint_sink_.submit_terminal_damage(damage);
    }
}

display::Rect TerminalApp::apply_console_update(text::TextConsoleUpdate update)
{
    if (pending_backing_scroll())
    {
        display::Rect dirty_rect;
        switch (update.action)
        {
        case text::TextConsoleAction::DrawGlyph:
            text_buffer_.put(update.cell.column, update.cell.row, update.glyph);
            dirty_rect = cell_rect(update.cell.column, update.cell.row);
            break;
        case text::TextConsoleAction::ClearCell:
            text_buffer_.clear_cell(update.cell.column, update.cell.row);
            dirty_rect = cell_rect(update.cell.column, update.cell.row);
            break;
        case text::TextConsoleAction::None:
            break;
        }

        record_pending_dirty(dirty_rect);
        if (update.scroll)
        {
            if (!text_buffer_.scroll_up())
            {
                render_cache_.invalidate();
                record_pending_dirty(bounds());
            }
            else
            {
                record_pending_scroll();
            }
        }
        return {};
    }

    display::Rect dirty_rect;
    const bool can_scroll_backing = update.scroll && render_cache_.valid();
    const bool can_render_backing_cell = !update.scroll && render_cache_.valid();

    switch (update.action)
    {
    case text::TextConsoleAction::DrawGlyph:
        text_buffer_.put(update.cell.column, update.cell.row, update.glyph);
        if (can_render_backing_cell)
        {
            render_text_cell(update.cell.column, update.cell.row, update.glyph);
            dirty_rect = cell_rect(update.cell.column, update.cell.row);
        }
        break;
    case text::TextConsoleAction::ClearCell:
        text_buffer_.clear_cell(update.cell.column, update.cell.row);
        if (can_render_backing_cell)
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
        display::Rect pre_scroll_dirty;
        if (can_scroll_backing && update.action == text::TextConsoleAction::DrawGlyph)
        {
            render_text_cell(update.cell.column, update.cell.row, update.glyph);
            pre_scroll_dirty = cell_rect(update.cell.column, update.cell.row);
        }
        if (can_scroll_backing && update.action == text::TextConsoleAction::ClearCell)
        {
            render_text_cell(update.cell.column, update.cell.row, text::kTextBufferBlank);
            pre_scroll_dirty = cell_rect(update.cell.column, update.cell.row);
        }
        if (can_scroll_backing)
        {
            pre_scroll_dirty = display::bounding_rect(pre_scroll_dirty, render_text_repaint(false));
        }

        if (!update_scope_active() && !pre_scroll_dirty.empty())
        {
            apply_repaint_request(repaint_.record_dirty(pre_scroll_dirty));
        }

        if (!text_buffer_.scroll_up())
        {
            render_text_repaint(true);
            apply_repaint_request(repaint_.record_dirty(bounds()));
            return {};
        }

        if (can_scroll_backing && update_scope_active())
        {
            record_pending_scroll();
            return {};
        }

        const display::Rect scroll_dirty = can_scroll_backing ? scroll_backing_text_grid_up(1)
                                                              : display::Rect{};
        if (scroll_dirty.empty())
        {
            render_text_repaint(true);
            apply_repaint_request(repaint_.record_dirty(bounds()));
            return {};
        }

        render_cache_.synchronize_from(text_buffer_);
        apply_repaint_request(repaint_.record_scroll(scroll_dirty, kCellHeight));
        return {};
    }

    return dirty_rect;
}

display::Rect TerminalApp::apply_write_character(char value)
{
    switch (value)
    {
    case '\n':
        return apply_console_update(console_.newline());
    case '\r':
        console_.carriage_return();
        return {};
    case '\b':
        return apply_console_update(console_.backspace());
    default:
        return apply_console_update(console_.write_char(value));
    }
}

void TerminalApp::write_tab()
{
    for (int index = 0; index < 4; ++index)
    {
        write_char(' ');
    }
}

void TerminalApp::clear()
{
    if (!ready())
    {
        return;
    }

    cursor_.hide();
    backing_.reset_scroll();
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

    const display::Rect old_bounds = caret_bounds();
    cursor_.show(console_.cursor_column(), console_.cursor_row());
    record_console_dirty(display::bounding_rect(old_bounds, caret_bounds()));
}

void TerminalApp::hide_cursor()
{
    if (!ready() || !cursor_.visible)
    {
        return;
    }

    const display::Rect dirty_rect = caret_bounds();
    cursor_.hide();
    record_console_dirty(dirty_rect);
}

void TerminalApp::write_char(char value)
{
    if (!ready())
    {
        return;
    }

    if (value == '\t')
    {
        write_tab();
        return;
    }

    record_console_dirty(apply_write_character(value));
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
