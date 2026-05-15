#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel
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

    [[nodiscard]] bool ready() const { return columns_ > 0 && rows_ > 0; }
    [[nodiscard]] uint64_t columns() const { return columns_; }
    [[nodiscard]] uint64_t rows() const { return rows_; }
    [[nodiscard]] uint64_t max_columns() const { return kTextBufferMaxColumns; }
    [[nodiscard]] uint64_t max_rows() const { return kTextBufferMaxRows; }

    [[nodiscard]] bool put(uint64_t column, uint64_t row, char glyph);
    [[nodiscard]] bool clear_cell(uint64_t column, uint64_t row);
    [[nodiscard]] bool scroll_up();
    [[nodiscard]] char glyph_at(uint64_t column, uint64_t row) const;

private:
    [[nodiscard]] bool in_bounds(uint64_t column, uint64_t row) const;
    [[nodiscard]] size_t index_of(uint64_t column, uint64_t row) const;

    char cells_[kTextBufferMaxColumns * kTextBufferMaxRows] = {};
    uint64_t columns_ = 0;
    uint64_t rows_ = 0;
};

} // namespace kernel
