#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel::text
{

inline constexpr uint64_t kTextBufferMaxColumns = 320;
inline constexpr uint64_t kTextBufferMaxRows = 128;
inline constexpr char kTextBufferBlank = '\0';

class TextBuffer
{
public:
    TextBuffer() = default;

    [[nodiscard]] bool reset(uint64_t columns, uint64_t rows);
    void clear();

    bool ready() const { return columns_ > 0 && rows_ > 0; }
    uint64_t columns() const { return columns_; }
    uint64_t rows() const { return rows_; }
    uint64_t max_columns() const { return kTextBufferMaxColumns; }
    uint64_t max_rows() const { return kTextBufferMaxRows; }

    bool put(uint64_t column, uint64_t row, char glyph);
    bool clear_cell(uint64_t column, uint64_t row);
    bool scroll_up();
    char glyph_at(uint64_t column, uint64_t row) const;

private:
    bool in_bounds(uint64_t column, uint64_t row) const;
    size_t index_of(uint64_t column, uint64_t row) const;

    char cells_[kTextBufferMaxColumns * kTextBufferMaxRows] = {};
    uint64_t columns_ = 0;
    uint64_t rows_ = 0;
}; // end class TextBuffer

} // namespace kernel::text
