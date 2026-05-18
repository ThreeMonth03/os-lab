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

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

uint64_t rows_for_line(uint64_t length, uint64_t columns)
{
    if (length == 0)
    {
        return 1;
    }

    return ((length - 1) / columns) + 1;
}

bool row_has_glyph(const TextBuffer & buffer, uint64_t row)
{
    for (uint64_t column = 0; column < buffer.columns(); ++column)
    {
        if (buffer.glyph_at(column, row) != kTextBufferBlank)
        {
            return true;
        }
    }

    return false;
}

uint64_t last_relevant_source_row(const TextBuffer & buffer, uint64_t cursor_row)
{
    uint64_t last_row = min_u64(cursor_row, buffer.rows() - 1);
    for (uint64_t row = buffer.rows(); row > 0; --row)
    {
        const uint64_t candidate = row - 1;
        if (row_has_glyph(buffer, candidate))
        {
            return max_u64(last_row, candidate);
        }
    }

    return last_row;
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

TextBuffer::ResizeResult TextBuffer::resize_reflowing_visible_content(
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

    columns_ = columns;
    rows_ = rows;
    clear();

    uint64_t top_virtual_row = 0;
    uint64_t next_line_virtual_row = 0;
    uint64_t cursor_virtual_row = 0;
    uint64_t cursor_virtual_column = 0;
    bool cursor_mapped = false;

    const auto make_virtual_row_visible = [this, &top_virtual_row](uint64_t virtual_row)
    {
        while (virtual_row >= top_virtual_row + rows_)
        {
            scroll_up();
            ++top_virtual_row;
        }
    };

    const auto set_virtual_row_continuation =
        [this, &make_virtual_row_visible, &top_virtual_row](uint64_t virtual_row,
                                                            bool continuation)
    {
        make_virtual_row_visible(virtual_row);
        const uint64_t destination_row = virtual_row - top_virtual_row;
        set_row_continuation(destination_row, destination_row != 0 && continuation);
    };

    const auto put_virtual_glyph =
        [this, &make_virtual_row_visible, &top_virtual_row](uint64_t virtual_row,
                                                            uint64_t column,
                                                            char glyph)
    {
        make_virtual_row_visible(virtual_row);
        if (glyph != kTextBufferBlank)
        {
            put(column, virtual_row - top_virtual_row, glyph);
        }
    };

    if (previous.ready())
    {
        const uint64_t last_source_row = last_relevant_source_row(previous, cursor_row);

        uint64_t source_row = 0;
        while (source_row <= last_source_row)
        {
            const uint64_t line_start_row = source_row;
            uint64_t line_end_row = line_start_row;
            while (line_end_row + 1 <= last_source_row &&
                   previous.row_continues_from_previous(line_end_row + 1))
            {
                ++line_end_row;
            }

            uint64_t last_row_length = 0;
            for (uint64_t column = previous.columns_; column > 0; --column)
            {
                if (previous.glyph_at(column - 1, line_end_row) != kTextBufferBlank)
                {
                    last_row_length = column;
                    break;
                }
            }

            uint64_t line_length =
                ((line_end_row - line_start_row) * previous.columns_) + last_row_length;
            const bool cursor_in_line = cursor_row >= line_start_row && cursor_row <= line_end_row;
            const uint64_t cursor_index = cursor_in_line
                                              ? ((cursor_row - line_start_row) * previous.columns_) +
                                                    min_u64(cursor_column, previous.columns_ - 1)
                                              : 0;
            if (cursor_in_line)
            {
                line_length = max_u64(line_length, cursor_index);
            }

            uint64_t line_rows = rows_for_line(line_length, columns_);
            if (cursor_in_line)
            {
                line_rows = max_u64(line_rows, (cursor_index / columns_) + 1);
            }

            for (uint64_t row = 0; row < line_rows; ++row)
            {
                set_virtual_row_continuation(next_line_virtual_row + row, row != 0);
            }

            for (uint64_t index = 0; index < line_length; ++index)
            {
                const uint64_t source_offset_row = index / previous.columns_;
                const uint64_t source_column = index % previous.columns_;
                const char glyph =
                    previous.glyph_at(source_column, line_start_row + source_offset_row);
                put_virtual_glyph(next_line_virtual_row + (index / columns_),
                                  index % columns_,
                                  glyph);
            }

            if (cursor_in_line)
            {
                cursor_virtual_row = next_line_virtual_row + (cursor_index / columns_);
                cursor_virtual_column = cursor_index % columns_;
                make_virtual_row_visible(cursor_virtual_row);
                cursor_mapped = true;
            }

            next_line_virtual_row += line_rows;
            source_row = line_end_row + 1;
        }
    }

    if (!cursor_mapped)
    {
        cursor_virtual_row = min_u64(cursor_row, rows - 1);
        cursor_virtual_column = min_u64(cursor_column, columns - 1);
        make_virtual_row_visible(cursor_virtual_row);
    }

    if (rows_ > 0)
    {
        row_continuation_[0] = false;
    }

    const uint64_t visible_cursor_row = cursor_virtual_row >= top_virtual_row
                                            ? min_u64(cursor_virtual_row - top_virtual_row, rows - 1)
                                            : 0;
    return {true, min_u64(cursor_virtual_column, columns - 1), visible_cursor_row};
}

void TextBuffer::clear()
{
    for (uint64_t row = 0; row < rows_; ++row)
    {
        for (uint64_t column = 0; column < columns_; ++column)
        {
            cells_[index_of(column, row)] = kTextBufferBlank;
        }
        row_continuation_[row] = false;
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
        row_continuation_[row - 1] = row_continuation_[row];
    }

    for (uint64_t column = 0; column < columns_; ++column)
    {
        cells_[index_of(column, rows_ - 1)] = kTextBufferBlank;
    }
    row_continuation_[rows_ - 1] = false;

    return true;
}

bool TextBuffer::set_row_continuation(uint64_t row, bool continuation)
{
    if (!ready() || row >= rows_)
    {
        return false;
    }

    row_continuation_[row] = row != 0 && continuation;
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

bool TextBuffer::row_continues_from_previous(uint64_t row) const
{
    return ready() && row < rows_ && row_continuation_[row];
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
