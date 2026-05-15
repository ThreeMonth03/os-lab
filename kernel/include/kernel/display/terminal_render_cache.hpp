#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/text/text_buffer.hpp"

namespace kernel::display
{

class TerminalRenderCache
{
public:
    TerminalRenderCache() = default;

    [[nodiscard]] bool reset(uint64_t columns, uint64_t rows);
    void invalidate();
    void clear_rendered();
    void synchronize_from(const TextBuffer & logical);

    bool valid() const { return valid_; }
    bool ready() const { return columns_ > 0 && rows_ > 0; }
    uint64_t columns() const { return columns_; }
    uint64_t rows() const { return rows_; }

    [[nodiscard]] bool mark_rendered(uint64_t column, uint64_t row, char glyph);
    [[nodiscard]] bool scroll_up(uint64_t rows);
    [[nodiscard]] bool needs_render(const TextBuffer & logical, uint64_t column, uint64_t row) const;
    [[nodiscard]] uint64_t count_dirty_cells(const TextBuffer & logical) const;
    [[nodiscard]] char glyph_at(uint64_t column, uint64_t row) const;

private:
    [[nodiscard]] bool in_bounds(uint64_t column, uint64_t row) const;
    [[nodiscard]] size_t index_of(uint64_t column, uint64_t row) const;

    char cells_[kTextBufferMaxColumns * kTextBufferMaxRows] = {};
    uint64_t columns_ = 0;
    uint64_t rows_ = 0;
    bool valid_ = false;
};

} // namespace kernel::display
