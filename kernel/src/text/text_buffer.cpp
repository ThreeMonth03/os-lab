#include "kernel/text/text_buffer.hpp"

namespace kernel
{

bool TextBuffer::reset(uint64_t columns, uint64_t rows)
{
    if (columns == 0 || rows == 0 || columns > kTextBufferMaxColumns || rows > kTextBufferMaxRows)
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

} // namespace kernel
