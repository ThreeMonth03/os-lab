#include "kernel/display/terminal_render_cache.hpp"

namespace kernel::display
{

bool TerminalRenderCache::reset(uint64_t columns, uint64_t rows)
{
    if (columns == 0 || rows == 0 || columns > kTextBufferMaxColumns || rows > kTextBufferMaxRows)
    {
        columns_ = 0;
        rows_ = 0;
        valid_ = false;
        return false;
    }

    columns_ = columns;
    rows_ = rows;
    invalidate();
    return true;
}

void TerminalRenderCache::invalidate()
{
    valid_ = false;
}

void TerminalRenderCache::clear_rendered()
{
    for (uint64_t row = 0; row < rows_; ++row)
    {
        for (uint64_t column = 0; column < columns_; ++column)
        {
            cells_[index_of(column, row)] = kTextBufferBlank;
        }
    }
    valid_ = ready();
}

void TerminalRenderCache::synchronize_from(const TextBuffer & logical)
{
    if (!ready() || !logical.ready() || logical.columns() != columns_ || logical.rows() != rows_)
    {
        invalidate();
        return;
    }

    for (uint64_t row = 0; row < rows_; ++row)
    {
        for (uint64_t column = 0; column < columns_; ++column)
        {
            cells_[index_of(column, row)] = logical.glyph_at(column, row);
        }
    }
    valid_ = true;
}

bool TerminalRenderCache::mark_rendered(uint64_t column, uint64_t row, char glyph)
{
    if (!valid_ || !in_bounds(column, row))
    {
        return false;
    }

    cells_[index_of(column, row)] = glyph;
    return true;
}

bool TerminalRenderCache::scroll_up(uint64_t rows)
{
    if (!valid_ || !ready() || rows == 0)
    {
        return false;
    }

    if (rows >= rows_)
    {
        invalidate();
        return false;
    }

    for (uint64_t row = rows; row < rows_; ++row)
    {
        for (uint64_t column = 0; column < columns_; ++column)
        {
            cells_[index_of(column, row - rows)] = cells_[index_of(column, row)];
        }
    }

    for (uint64_t row = rows_ - rows; row < rows_; ++row)
    {
        for (uint64_t column = 0; column < columns_; ++column)
        {
            cells_[index_of(column, row)] = kTextBufferBlank;
        }
    }

    return true;
}

bool TerminalRenderCache::needs_render(const TextBuffer & logical, uint64_t column, uint64_t row) const
{
    if (!valid_ || !in_bounds(column, row))
    {
        return false;
    }

    return logical.glyph_at(column, row) != cells_[index_of(column, row)];
}

uint64_t TerminalRenderCache::count_dirty_cells(const TextBuffer & logical) const
{
    if (!valid_ || !ready() || !logical.ready() || logical.columns() != columns_ || logical.rows() != rows_)
    {
        return columns_ * rows_;
    }

    uint64_t count = 0;
    for (uint64_t row = 0; row < rows_; ++row)
    {
        for (uint64_t column = 0; column < columns_; ++column)
        {
            if (needs_render(logical, column, row))
            {
                ++count;
            }
        }
    }
    return count;
}

char TerminalRenderCache::glyph_at(uint64_t column, uint64_t row) const
{
    if (!valid_ || !in_bounds(column, row))
    {
        return kTextBufferBlank;
    }

    return cells_[index_of(column, row)];
}

bool TerminalRenderCache::in_bounds(uint64_t column, uint64_t row) const
{
    return ready() && column < columns_ && row < rows_;
}

size_t TerminalRenderCache::index_of(uint64_t column, uint64_t row) const
{
    return static_cast<size_t>((row * columns_) + column);
}

} // namespace kernel::display
