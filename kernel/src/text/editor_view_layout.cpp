#include "kernel/text/editor_view_layout.hpp"

namespace kernel::text
{

EditorViewLayout::EditorViewLayout(uint64_t columns, uint64_t prompt_column, uint64_t prompt_width)
    : columns_(columns)
    , prompt_column_(prompt_column)
    , prompt_width_(prompt_width)
{
}

EditorViewCell EditorViewLayout::position_for(size_t text_index) const
{
    if (!ready())
    {
        return {};
    }

    const uint64_t offset = prompt_column_ + prompt_width_ + static_cast<uint64_t>(text_index);
    return {offset % columns_, offset / columns_};
}

uint64_t EditorViewLayout::visual_rows(size_t text_length) const
{
    if (!ready())
    {
        return 1;
    }

    return position_for(text_length).row + 1;
}

uint64_t EditorViewLayout::prompt_row_for_terminal_cursor(uint64_t terminal_cursor_row,
                                                          size_t text_cursor) const
{
    const EditorViewCell cursor_offset = position_for(text_cursor);
    return terminal_cursor_row >= cursor_offset.row ? terminal_cursor_row - cursor_offset.row : 0;
}

} // namespace kernel::text
