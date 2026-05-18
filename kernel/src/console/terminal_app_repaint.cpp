#include "terminal_app.hpp"

namespace kernel::console
{

void TerminalApp::repaint_text_layer()
{
    backing_.reset_scroll();
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
    if (!render_cache_.mark_rendered(column, row, glyph))
    {
        render_cache_.invalidate();
    }
}

void TerminalApp::render_buffer_cell(uint64_t column, uint64_t row)
{
    render_text_cell(column, row, text_buffer_.glyph_at(column, row));
}

display::Rect TerminalApp::scroll_backing_text_grid_up(uint64_t rows)
{
    const display::Rect grid = text_grid_rect();
    if (grid.empty() || rows == 0 || grid.height <= kCellHeight)
    {
        return {};
    }

    if (rows >= text_buffer_.rows())
    {
        backing_.reset_scroll();
        renderer_.clear_rect(grid);
        return grid;
    }

    const uint64_t distance = rows * kCellHeight;
    backing_.scroll_up(distance);

    const display::Rect exposed = {
        grid.x,
        grid.y + grid.height - distance,
        grid.width,
        distance,
    };
    renderer_.clear_rect(exposed);
    return grid;
}

display::Rect TerminalApp::render_text_cells_in_rect(display::Rect rect)
{
    rect = display::intersect_rect(rect, text_grid_rect());
    if (rect.empty())
    {
        return {};
    }

    const uint64_t first_column = (rect.x - text_viewport_.x) / kCellWidth;
    const uint64_t first_row = (rect.y - text_viewport_.y) / kCellHeight;
    const uint64_t last_column = (rect.x + rect.width - 1 - text_viewport_.x) / kCellWidth;
    const uint64_t last_row = (rect.y + rect.height - 1 - text_viewport_.y) / kCellHeight;

    display::Rect dirty_rect;
    for (uint64_t row = first_row; row <= last_row && row < text_buffer_.rows(); ++row)
    {
        for (uint64_t column = first_column; column <= last_column && column < text_buffer_.columns();
             ++column)
        {
            render_buffer_cell(column, row);
            dirty_rect = display::bounding_rect(dirty_rect, cell_rect(column, row));
        }
    }
    return dirty_rect;
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

display::Rect TerminalApp::render_text_repaint(bool repaint_entire_layer)
{
    if (repaint_entire_layer || !render_cache_.valid())
    {
        repaint_text_layer();
        return bounds();
    }

    return render_dirty_text_cells();
}

void TerminalApp::apply_repaint(display::FrameDamage damage)
{
    if (pending_backing_scroll())
    {
        if (damage.has_dirty())
        {
            record_pending_dirty(damage.dirty_rect);
        }
        return;
    }

    const display::Rect backing_dirty = render_text_repaint(false);
    const bool scroll_related_backing_dirty = damage.has_scroll();
    if (!backing_dirty.empty() && !damage.append_dirty(backing_dirty, scroll_related_backing_dirty))
    {
        damage = {display::bounding_rect(damage.dirty_rect, backing_dirty), {}};
    }

    if (damage.empty())
    {
        return;
    }

    if (repaint_sink_.submit_app_surface_damage != nullptr)
    {
        repaint_sink_.submit_app_surface_damage(damage);
    }
}

void TerminalApp::apply_repaint_request(display::TerminalRepaintRequest request)
{
    apply_repaint(request.damage);
}

void TerminalApp::record_console_dirty(display::Rect dirty_rect)
{
    apply_repaint_request(repaint_.record_dirty(dirty_rect));
}

} // namespace kernel::console
