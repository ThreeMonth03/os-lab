#include "terminal_app.hpp"

namespace kernel::console
{

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

void TerminalApp::hide_text_cursor()
{
    if (!cursor_.visible)
    {
        return;
    }

    renderer_.erase_cursor(cursor_.column, cursor_.row);
    cursor_.hide();
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
    if (!render_cache_.mark_rendered(column, row, glyph))
    {
        render_cache_.invalidate();
    }
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

display::Rect TerminalApp::render_text_repaint(bool repaint_entire_layer)
{
    if (repaint_entire_layer || !render_cache_.valid())
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

    if (cursor_.visible && display::rects_overlap(cell_rect(cursor_.column, cursor_.row), dirty_rect))
    {
        renderer_.draw_cursor(cursor_.column, cursor_.row);
    }
}

void TerminalApp::apply_repaint(display::Rect dirty_rect,
                                bool repaint_text_layer,
                                bool repaint_entire_text_layer,
                                bool repaint_higher_layers)
{
    if (repaint_text_layer)
    {
        dirty_rect =
            display::bounding_rect(dirty_rect, render_text_repaint(repaint_entire_text_layer));
    }

    if (repaint_higher_layers)
    {
        repaint_layers_above(dirty_rect);
    }
}

void TerminalApp::apply_repaint_request(display::TerminalRepaintRequest request)
{
    apply_repaint(request.dirty_rect,
                  request.repaint_text_layer,
                  request.repaint_entire_text_layer,
                  request.repaint_higher_layers);
}

void TerminalApp::apply_repaint_flush(display::TerminalRepaintFlush flush)
{
    apply_repaint(flush.dirty_rect,
                  flush.repaint_text_layer,
                  flush.repaint_entire_text_layer,
                  flush.repaint_higher_layers);
}

void TerminalApp::record_console_dirty(display::Rect dirty_rect)
{
    apply_repaint_request(repaint_.record_dirty(dirty_rect));
}

} // namespace kernel::console
