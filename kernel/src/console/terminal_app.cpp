#include "terminal_app.hpp"

#include <stddef.h>

#include "kernel/memory/heap.hpp"

#ifndef OS_LAB_TERMINAL_WINDOW_CHROME
#define OS_LAB_TERMINAL_WINDOW_CHROME 0
#endif

namespace kernel::console
{

namespace
{

display::WindowFrameConfig terminal_window_frame_config()
{
    return display::terminal_window_frame_config(OS_LAB_TERMINAL_WINDOW_CHROME != 0);
}

display::Rect text_grid_rect_for(display::Rect viewport, display::AppCellCapacity capacity)
{
    return {
        viewport.x,
        viewport.y,
        capacity.columns * TerminalApp::kCellWidth,
        capacity.rows * TerminalApp::kCellHeight,
    };
}

bool should_preserve_text(TerminalResizePolicy policy)
{
    return policy == TerminalResizePolicy::PreserveVisibleContent;
}

bool line_break_is_continuation(text::TextConsoleLineBreak line_break)
{
    return line_break == text::TextConsoleLineBreak::SoftWrap;
}

void apply_line_break_metadata(text::TextBuffer & buffer, text::TextConsoleUpdate update)
{
    if (update.line_break == text::TextConsoleLineBreak::None)
    {
        return;
    }

    buffer.set_row_continuation(update.line_break_row,
                                line_break_is_continuation(update.line_break));
}

} // namespace

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

    if (!app_surface.valid() || !repaint_sink.ready())
    {
        return false;
    }

    repaint_sink_ = repaint_sink;
    foreground_ = foreground;
    background_ = background;
    return replace_surface(app_surface, TerminalResizePolicy::Clear);
}

bool TerminalApp::ready() const
{
    return renderer_.ready() && backing_.ready() && app_surface_.valid() && !app_surface_.closed();
}

bool TerminalApp::allocate_backing_surface_for(display::AppSurface app_surface,
                                               uint32_t *& memory,
                                               size_t & bytes,
                                               display::BackingSurface & backing_storage,
                                               display::ScrollMappedSurface & backing) const
{
    const display::WindowFrameMetrics metrics = frame_metrics_for(app_surface.bounds);
    const display::AppCellCapacity capacity = cell_capacity_for(app_surface);
    if (!capacity.valid())
    {
        return false;
    }

    size_t required_bytes = 0;
    if (!display::backing_surface_required_bytes(app_surface.bounds, required_bytes))
    {
        return false;
    }

    void * allocated_memory = kernel::memory::heap::allocate(required_bytes, alignof(uint32_t));
    if (allocated_memory == nullptr)
    {
        return false;
    }

    memory = static_cast<uint32_t *>(allocated_memory);
    bytes = required_bytes;
    backing_storage = display::BackingSurface(memory, app_surface.bounds, app_surface.bounds.width);
    backing.reset(backing_storage, text_grid_rect_for(metrics.client_bounds, capacity));
    return backing.ready();
}

bool TerminalApp::replace_surface(display::AppSurface app_surface, TerminalResizePolicy policy)
{
    const display::WindowFrameMetrics metrics = frame_metrics_for(app_surface.bounds);
    const display::AppCellCapacity capacity = cell_capacity_for(app_surface);
    if (!app_surface.valid() || app_surface.closed() || !capacity.valid() ||
        capacity.columns > text_buffer_.max_columns() || capacity.rows > text_buffer_.max_rows())
    {
        return false;
    }

    uint32_t * new_memory = nullptr;
    size_t new_bytes = 0;
    display::BackingSurface new_storage;
    display::ScrollMappedSurface new_backing;
    if (!allocate_backing_surface_for(app_surface, new_memory, new_bytes, new_storage, new_backing))
    {
        return false;
    }

    const bool preserve_text = should_preserve_text(policy) && text_buffer_.ready();
    const uint64_t previous_cursor_column = console_.cursor_column();
    const uint64_t previous_cursor_row = console_.cursor_row();
    const bool previous_cursor_visible = cursor_.visible;
    if (preserve_text)
    {
        resize_snapshot_ = text_buffer_;
    }

    if (backing_memory_ != nullptr && !kernel::memory::heap::free(backing_memory_))
    {
        if (!kernel::memory::heap::free(new_memory))
        {
            return false;
        }
        return false;
    }

    app_surface_ = app_surface;
    frame_metrics_ = metrics;
    text_viewport_ = frame_metrics_.client_bounds;
    backing_memory_ = new_memory;
    backing_bytes_ = new_bytes;
    backing_storage_ = new_storage;
    backing_.reset(backing_storage_, text_grid_rect_for(text_viewport_, capacity));

    text::TextBuffer::ResizeResult resize_result;
    if (preserve_text)
    {
        resize_result = text_buffer_.resize_reflowing_visible_content(resize_snapshot_,
                                                                      capacity.columns,
                                                                      capacity.rows,
                                                                      previous_cursor_column,
                                                                      previous_cursor_row);
    }
    else
    {
        resize_result.resized = text_buffer_.reset(capacity.columns, capacity.rows);
    }

    if (!resize_result.resized || !render_cache_.reset(capacity.columns, capacity.rows))
    {
        return false;
    }
    console_.reset(capacity.columns, capacity.rows);
    console_.set_cursor(resize_result.cursor_column, resize_result.cursor_row);
    renderer_.reset(backing_, text_viewport_, foreground_, background_);
    repaint_.reset(app_surface_.bounds);
    cursor_.reset();
    if (previous_cursor_visible && app_surface_.visible())
    {
        cursor_.show(console_.cursor_column(), console_.cursor_row());
    }
    update_depth_ = 0;
    pending_scroll_rows_ = 0;
    pending_dirty_after_scroll_ = {};
    backing_.fill_rect(app_surface_.bounds, background_);
    paint_window_chrome();
    renderer_.clear_screen();
    repaint_text_layer();
    return renderer_.ready();
}

bool TerminalApp::move_surface(display::AppSurface app_surface)
{
    if (!ready() || !app_surface.valid() || app_surface.closed() ||
        app_surface.bounds.width != app_surface_.bounds.width ||
        app_surface.bounds.height != app_surface_.bounds.height)
    {
        return false;
    }

    const display::WindowFrameMetrics metrics = frame_metrics_for(app_surface.bounds);
    const display::AppCellCapacity capacity = cell_capacity_for(app_surface);
    if (!metrics.valid() || !capacity.valid() || capacity.columns != text_buffer_.columns() ||
        capacity.rows != text_buffer_.rows() || !backing_storage_.move_to(app_surface.bounds))
    {
        return false;
    }

    app_surface_ = app_surface;
    frame_metrics_ = metrics;
    text_viewport_ = frame_metrics_.client_bounds;
    if (!backing_.reset_preserving_scroll(backing_storage_,
                                          text_grid_rect_for(text_viewport_, capacity)))
    {
        return false;
    }
    renderer_.reset(backing_, text_viewport_, foreground_, background_);
    repaint_.reset(app_surface_.bounds);
    return renderer_.ready();
}

bool TerminalApp::resize(display::AppSurface app_surface, TerminalResizePolicy policy)
{
    if (!ready() || !repaint_sink_.ready())
    {
        return false;
    }

    if (!replace_surface(app_surface, policy))
    {
        return false;
    }

    return true;
}

void TerminalApp::sync_surface_state(display::AppSurface app_surface)
{
    if (!app_surface.valid())
    {
        return;
    }

    if (ready() && app_surface.visible() && app_surface_.visible() &&
        app_surface.bounds.width == app_surface_.bounds.width &&
        app_surface.bounds.height == app_surface_.bounds.height &&
        (app_surface.bounds.x != app_surface_.bounds.x ||
         app_surface.bounds.y != app_surface_.bounds.y))
    {
        if (move_surface(app_surface))
        {
            return;
        }
    }

    const bool chrome_state_changed =
        ready() &&
        (app_surface.focused != app_surface_.focused || app_surface.active != app_surface_.active);
    app_surface_ = app_surface;
    if (!app_surface_.visible() || app_surface_.closed())
    {
        cursor_.hide();
        return;
    }
    if (chrome_state_changed && frame_metrics_.visible)
    {
        paint_window_chrome();
    }
}

display::PixelSample TerminalApp::sample_pixel(uint64_t x, uint64_t y) const
{
    return ready() && app_surface_.visible() ? backing_.sample(x, y) : display::transparent_pixel();
}

display::PixelSample TerminalApp::sample_caret_pixel(uint64_t x, uint64_t y) const
{
    return ready() && app_surface_.visible() && app_surface_.focused && cursor_.visible &&
                   !display::intersect_rect(caret_bounds(), {x, y, 1, 1}).empty()
               ? display::opaque_pixel(renderer_.foreground())
               : display::transparent_pixel();
}

const uint32_t * TerminalApp::row_pixels(uint64_t y) const
{
    return ready() && app_surface_.visible() ? backing_.row_pixels(y) : nullptr;
}

uint64_t TerminalApp::text_grid_width() const
{
    return text_buffer_.columns() * kCellWidth;
}

uint64_t TerminalApp::text_grid_height() const
{
    return text_buffer_.rows() * kCellHeight;
}

display::WindowFrameMetrics TerminalApp::frame_metrics_for(display::Rect bounds) const
{
    return display::WindowChrome::metrics_for(bounds, terminal_window_frame_config());
}

display::AppCellCapacity TerminalApp::cell_capacity_for(display::AppSurface app_surface) const
{
    const display::WindowFrameMetrics metrics = frame_metrics_for(app_surface.bounds);
    return metrics.valid()
               ? display::DesktopAppLayout::cell_capacity_for(metrics.client_bounds,
                                                              kCellWidth,
                                                              kCellHeight)
               : display::AppCellCapacity{};
}

display::WindowChromeVisualState TerminalApp::chrome_visual_state() const
{
    return display::WindowChrome::visual_state_for(app_surface_.active, app_surface_.focused);
}

display::Rect TerminalApp::text_grid_rect() const
{
    return {
        text_viewport_.x,
        text_viewport_.y,
        text_grid_width(),
        text_grid_height(),
    };
}

bool TerminalApp::chrome_stroke_pixel(uint64_t x, uint64_t y) const
{
    switch (chrome_visual_state())
    {
    case display::WindowChromeVisualState::Focused:
        return true;
    case display::WindowChromeVisualState::Active:
        return (x % 2) == 0;
    case display::WindowChromeVisualState::Inactive:
        return ((x + y) % 2) == 0;
    }
    return true;
}

void TerminalApp::paint_chrome_rect(display::Rect rect)
{
    rect = display::intersect_rect(rect, app_surface_.bounds);
    if (rect.empty())
    {
        return;
    }

    for (uint64_t y = 0; y < rect.height; ++y)
    {
        for (uint64_t x = 0; x < rect.width; ++x)
        {
            const uint64_t pixel_x = rect.x + x;
            const uint64_t pixel_y = rect.y + y;
            backing_.put_pixel(pixel_x,
                               pixel_y,
                               chrome_stroke_pixel(pixel_x, pixel_y) ? foreground_ : background_);
        }
    }
}

void TerminalApp::paint_window_chrome()
{
    if (!backing_.ready())
    {
        return;
    }

    if (!frame_metrics_.visible)
    {
        return;
    }

    const display::Rect outer = frame_metrics_.outer_bounds;
    const uint64_t border = frame_metrics_.border_thickness;
    if (border == 0)
    {
        return;
    }

    paint_chrome_rect({outer.x, outer.y, outer.width, border});
    paint_chrome_rect({outer.x, outer.y + outer.height - border, outer.width, border});
    paint_chrome_rect({outer.x, outer.y, border, outer.height});
    paint_chrome_rect({outer.x + outer.width - border, outer.y, border, outer.height});

    const display::Rect title = frame_metrics_.title_bar_bounds;
    if (!title.empty())
    {
        paint_chrome_rect({title.x, title.y + title.height - border, title.width, border});
    }

    const display::Rect close = frame_metrics_.close_button_bounds;
    if (!close.empty())
    {
        paint_chrome_rect({close.x, close.y, close.width, border});
        paint_chrome_rect({close.x, close.y + close.height - border, close.width, border});
        paint_chrome_rect({close.x, close.y, border, close.height});
        paint_chrome_rect({close.x + close.width - border, close.y, border, close.height});

        for (uint64_t y = 0; y < close.height; ++y)
        {
            for (uint64_t x = 0; x < close.width; ++x)
            {
                const uint64_t pixel_x = close.x + x;
                const uint64_t pixel_y = close.y + y;
                if (display::WindowChrome::close_button_icon_contains_pixel(frame_metrics_,
                                                                            pixel_x,
                                                                            pixel_y))
                {
                    backing_.put_pixel(pixel_x,
                                       pixel_y,
                                       chrome_stroke_pixel(pixel_x, pixel_y) ? foreground_
                                                                             : background_);
                }
            }
        }
    }

    const display::Rect handle = frame_metrics_.resize_handle_bounds;
    if (!handle.empty())
    {
        paint_chrome_rect({handle.x + handle.width - border, handle.y, border, handle.height});
        paint_chrome_rect({handle.x, handle.y + handle.height - border, handle.width, border});
    }
}

display::Rect TerminalApp::cell_rect(uint64_t column, uint64_t row) const
{
    return {
        text_viewport_.x + (column * kCellWidth),
        text_viewport_.y + (row * kCellHeight),
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
        text_viewport_.x + (column * kCellWidth),
        text_viewport_.y + (row * kCellHeight) + kCursorTop,
        text::Glyph5x7::width * display::TerminalRenderer::kGlyphScale,
        kCursorHeight,
    };
}

display::Rect TerminalApp::caret_bounds() const
{
    return ready() && app_surface_.visible() && app_surface_.focused && cursor_.visible
               ? caret_rect(cursor_.column, cursor_.row)
               : display::Rect{};
}

display::Rect TerminalApp::row_tail_rect(uint64_t column, uint64_t row) const
{
    if (column >= console_.columns())
    {
        return {};
    }

    return {
        text_viewport_.x + (column * kCellWidth),
        text_viewport_.y + (row * kCellHeight),
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
    pending_dirty_after_scroll_ = display::scroll_dirty_region_up(pending_dirty_after_scroll_,
                                                                  text_grid_rect(),
                                                                  kCellHeight);

    if (pending_scroll_rows_ < text_buffer_.rows())
    {
        ++pending_scroll_rows_;
    }

    record_pending_dirty(display::exposed_scroll_region({text_grid_rect(), kCellHeight}));
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
    display::Rect rendered_dirty =
        render_text_cells_in_rect(display::bounding_rect(pending_dirty, exposed));
    rendered_dirty = display::bounding_rect(rendered_dirty, render_dirty_text_cells());

    display::FrameDamage damage;
    if (!damage.append_scroll({scroll_dirty, distance}) ||
        !damage.append_dirty(exposed, true) ||
        !damage.append_dirty(rendered_dirty))
    {
        apply_repaint_request(repaint_.record_dirty(bounds()));
        return;
    }

    if (repaint_sink_.submit_app_surface_damage != nullptr)
    {
        repaint_sink_.submit_app_surface_damage(damage);
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
                apply_line_break_metadata(text_buffer_, update);
                record_pending_scroll();
            }
        }
        else
        {
            apply_line_break_metadata(text_buffer_, update);
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
        apply_line_break_metadata(text_buffer_, update);

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

    apply_line_break_metadata(text_buffer_, update);
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
    if (column == 0)
    {
        text_buffer_.set_row_continuation(row, false);
    }
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

void TerminalApp::set_row_continuation(uint64_t row, bool continuation)
{
    text_buffer_.set_row_continuation(row, continuation);
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
