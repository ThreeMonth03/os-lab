#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct EditorViewCell {
    uint64_t column = 0;
    uint64_t row = 0;
};

class EditorViewLayout {
  public:
    EditorViewLayout() = default;
    EditorViewLayout(uint64_t columns, uint64_t prompt_column, uint64_t prompt_width);

    [[nodiscard]] bool ready() const { return columns_ > 0; }
    [[nodiscard]] uint64_t columns() const { return columns_; }
    [[nodiscard]] uint64_t prompt_column() const { return prompt_column_; }
    [[nodiscard]] uint64_t prompt_width() const { return prompt_width_; }

    [[nodiscard]] EditorViewCell position_for(size_t text_index) const;
    [[nodiscard]] uint64_t visual_rows(size_t text_length) const;

  private:
    uint64_t columns_ = 0;
    uint64_t prompt_column_ = 0;
    uint64_t prompt_width_ = 0;
};

} // namespace kernel
