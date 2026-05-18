#include "kernel/text/text_buffer.hpp"

namespace kernel::text
{

namespace
{

bool valid_dimensions(uint64_t columns, uint64_t rows)
{
    return columns != 0 && rows != 0 && columns <= kTextBufferMaxColumns &&
           rows <= kTextBufferMaxRows;
}

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

} // namespace

bool TextBuffer::reset(uint64_t columns, uint64_t rows)
{
    if (!valid_dimensions(columns, rows))
    {
        columns_ = 0;
        rows_ = 0;
        return false;
    }

    columns_ = columns;
    rows_ = rows;
    clear();
    return true;
}

TextBuffer::ResizeResult TextBuffer::resize_preserving_visible_content(
    const TextBuffer & previous,
    uint64_t columns,
    uint64_t rows,
    uint64_t cursor_column,
    uint64_t cursor_row)
{
    if (&previous == this || !valid_dimensions(columns, rows))
    {
        return {};
    }

    const uint64_t next_cursor_column = min_u64(cursor_column, columns - 1);
    const uint64_t next_cursor_row = min_u64(cursor_row, rows - 1);

    columns_ = columns;
    rows_ = rows;
    clear();

    if (previous.ready())
    {
        const uint64_t anchor_source_row = min_u64(cursor_row, previous.rows_ - 1);
        const uint64_t anchor_destination_row = min_u64(anchor_source_row, rows - 1);
        const uint64_t source_start_row = anchor_source_row - anchor_destination_row;
        const uint64_t rows_to_copy = min_u64(previous.rows_ - source_start_row, rows);
        const uint64_t columns_to_copy = min_u64(previous.columns_, columns);

        for (uint64_t row = 0; row < rows_to_copy; ++row)
        {
            for (uint64_t column = 0; column < columns_to_copy; ++column)
            {
                cells_[index_of(column, row)] =
                    previous.glyph_at(column, source_start_row + row);
            }
        }
    }

    return {true, next_cursor_column, next_cursor_row};
}

void TextBuffer::clear()
{
    for (uint64_t row = 0; row < rows_; ++row)
    {
        for (uint64_t column = 0; column < columns_; ++column)
        {
            cells_[index_of(column, row)] = kTextBufferBlank;
        }
    }
}

bool TextBuffer::put(uint64_t column, uint64_t row, char glyph)
{
    if (!in_bounds(column, row))
    {
        return false;
    }

    cells_[index_of(column, row)] = glyph;
    return true;
}

bool TextBuffer::clear_cell(uint64_t column, uint64_t row)
{
    return put(column, row, kTextBufferBlank);
}

bool TextBuffer::scroll_up()
{
    if (!ready())
    {
        return false;
    }

    for (uint64_t row = 1; row < rows_; ++row)
    {
        for (uint64_t column = 0; column < columns_; ++column)
        {
            cells_[index_of(column, row - 1)] = cells_[index_of(column, row)];
        }
    }

    for (uint64_t column = 0; column < columns_; ++column)
    {
        cells_[index_of(column, rows_ - 1)] = kTextBufferBlank;
    }

    return true;
}

char TextBuffer::glyph_at(uint64_t column, uint64_t row) const
{
    if (!in_bounds(column, row))
    {
        return kTextBufferBlank;
    }

    return cells_[index_of(column, row)];
}

bool TextBuffer::in_bounds(uint64_t column, uint64_t row) const
{
    return ready() && column < columns_ && row < rows_;
}

size_t TextBuffer::index_of(uint64_t column, uint64_t row) const
{
    return static_cast<size_t>((row * columns_) + column);
}

} // namespace kernel::text
