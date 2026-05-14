#include "kernel/editor_view_layout.hpp"

namespace kernel {

EditorViewLayout::EditorViewLayout(uint64_t columns, uint64_t prompt_column, uint64_t prompt_width)
    : columns_(columns), prompt_column_(prompt_column), prompt_width_(prompt_width) {}

EditorViewCell EditorViewLayout::position_for(size_t text_index) const {
    if (!ready()) {
        return {};
    }

    const uint64_t offset = prompt_column_ + prompt_width_ + static_cast<uint64_t>(text_index);
    return {offset % columns_, offset / columns_};
}

uint64_t EditorViewLayout::visual_rows(size_t text_length) const {
    if (!ready()) {
        return 1;
    }

    return position_for(text_length).row + 1;
}

} // namespace kernel
