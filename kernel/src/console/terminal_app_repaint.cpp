#include "terminal_app.hpp"

namespace kernel::console
{

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

display::Rect TerminalApp::scroll_backing_text_grid_up()
{
    const display::Rect grid = text_grid_rect();
    if (grid.empty() || grid.height <= kCellHeight)
    {
        return {};
    }

    const display::Rect source = {
        grid.x,
        grid.y + kCellHeight,
        grid.width,
        grid.height - kCellHeight,
    };
    const display::Rect moved = backing_.copy_rect(source, grid.x, grid.y);

    const display::Rect exposed = {
        grid.x,
        grid.y + grid.height - kCellHeight,
        grid.width,
        kCellHeight,
    };
    renderer_.clear_rect(exposed);
    return display::bounding_rect(moved, exposed);
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
    const display::Rect backing_dirty = render_text_repaint(false);
    if (!backing_dirty.empty() && !damage.append_dirty(backing_dirty))
    {
        damage = {display::bounding_rect(damage.dirty_rect, backing_dirty), {}};
    }

    if (damage.empty())
    {
        return;
    }

    if (repaint_sink_.submit_terminal_damage != nullptr)
    {
        repaint_sink_.submit_terminal_damage(damage);
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
