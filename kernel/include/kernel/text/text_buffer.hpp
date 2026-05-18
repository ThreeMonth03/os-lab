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
    struct ResizeResult
    {
        bool resized = false;
        uint64_t cursor_column = 0;
        uint64_t cursor_row = 0;
    };

    TextBuffer() = default;

    [[nodiscard]] bool reset(uint64_t columns, uint64_t rows);
    [[nodiscard]] ResizeResult resize_reflowing_visible_content(const TextBuffer & previous,
                                                                uint64_t columns,
                                                                uint64_t rows,
                                                                uint64_t cursor_column,
                                                                uint64_t cursor_row);
    void clear();

    bool ready() const { return columns_ > 0 && rows_ > 0; }
    uint64_t columns() const { return columns_; }
    uint64_t rows() const { return rows_; }
    uint64_t max_columns() const { return kTextBufferMaxColumns; }
    uint64_t max_rows() const { return kTextBufferMaxRows; }

    bool put(uint64_t column, uint64_t row, char glyph);
    bool clear_cell(uint64_t column, uint64_t row);
    bool scroll_up();
    bool set_row_continuation(uint64_t row, bool continuation);
    char glyph_at(uint64_t column, uint64_t row) const;
    bool row_continues_from_previous(uint64_t row) const;

private:
    bool in_bounds(uint64_t column, uint64_t row) const;
    size_t index_of(uint64_t column, uint64_t row) const;

    char cells_[kTextBufferMaxColumns * kTextBufferMaxRows] = {};
    bool row_continuation_[kTextBufferMaxRows] = {};
    uint64_t columns_ = 0;
    uint64_t rows_ = 0;
}; // end class TextBuffer

} // namespace kernel::text
