#include "shell_editor_view.hpp"

#include "kernel/console/terminal.hpp"

namespace
{

namespace terminal = kernel::console::terminal;

constexpr kernel::StringView kDefaultPrompt = "> ";
constexpr kernel::StringView kCapsPrompt = "[CAPS] > ";

uint64_t max_u64(uint64_t left, uint64_t right) { return left > right ? left : right; }

} // namespace

namespace kernel::shell
{

text::EditorSnapshot EditorView::snapshot(const text::LineEditor & line, bool caps_lock) const
{
    return {line.cursor(), line.size(), prompt_for_caps(caps_lock).size()};
}

StringView EditorView::prompt_for_caps(bool caps_lock)
{
    return caps_lock ? kCapsPrompt : kDefaultPrompt;
}

text::EditorViewLayout EditorView::layout(bool caps_lock) const
{
    return {terminal::columns(), position_.prompt_column, prompt_for_caps(caps_lock).size()};
}

void EditorView::scroll_to_fit(uint64_t rows_needed)
{
    const uint64_t rows = terminal::rows();
    if (rows == 0)
    {
        return;
    }

    const uint64_t target_prompt_row = rows_needed >= rows ? 0 : rows - rows_needed;
    if (position_.prompt_row <= target_prompt_row)
    {
        return;
    }

    const uint64_t scroll_count = position_.prompt_row - target_prompt_row;
    for (uint64_t count = 0; count < scroll_count; ++count)
    {
        terminal::set_cursor(0, rows - 1);
        terminal::write_char('\n');
    }

    position_.prompt_row = target_prompt_row;
}

void EditorView::clear_rendered_area(uint64_t rows_to_clear) const
{
    const uint64_t rows = terminal::rows();
    for (uint64_t row = 0; row < rows_to_clear && position_.prompt_row + row < rows; ++row)
    {
        const uint64_t column = row == 0 ? position_.prompt_column : 0;
        terminal::clear_row_from(column, position_.prompt_row + row);
    }
}

void EditorView::mark_line_continuations(uint64_t rows_to_cover, uint64_t rows_needed) const
{
    const uint64_t rows = terminal::rows();
    for (uint64_t row = 0; row < rows_to_cover && position_.prompt_row + row < rows; ++row)
    {
        terminal::set_row_continuation(position_.prompt_row + row, row != 0 && row < rows_needed);
    }
}

void EditorView::set_cursor(bool caps_lock, size_t index) const
{
    const text::EditorViewCell cell = layout(caps_lock).position_for(index);
    terminal::set_cursor(cell.column, position_.prompt_row + cell.row);
}

void EditorView::draw_text_range(StringView text, const text::EditorViewLayout & current_layout, size_t start, size_t end) const
{
    for (size_t index = start; index < end && index < text.size(); ++index)
    {
        const text::EditorViewCell cell = current_layout.position_for(index);
        terminal::draw_char_at(cell.column, position_.prompt_row + cell.row, text[index]);
    }
}

void EditorView::clear_text_range(const text::EditorViewLayout & current_layout, size_t start, size_t end) const
{
    for (size_t index = start; index < end; ++index)
    {
        const text::EditorViewCell cell = current_layout.position_for(index);
        terminal::clear_cell_at(cell.column, position_.prompt_row + cell.row);
    }
}

void EditorView::redraw_prompt_and_line(const text::LineEditor & line, bool caps_lock)
{
    terminal::hide_cursor();

    const StringView prompt = prompt_for_caps(caps_lock);
    const text::EditorViewLayout current_layout = layout(caps_lock);
    const uint64_t rows_needed = current_layout.visual_rows(line.view().size());

    scroll_to_fit(rows_needed);
    clear_rendered_area(max_u64(position_.rendered_rows, rows_needed));

    terminal::set_cursor(position_.prompt_column, position_.prompt_row);
    terminal::write_string(prompt);
    position_.input_column = terminal::cursor_column();
    position_.input_row = terminal::cursor_row();
    terminal::write_string(line.view());
    mark_line_continuations(max_u64(position_.rendered_rows, rows_needed), rows_needed);
    position_.rendered_rows = rows_needed;

    set_cursor(caps_lock, line.cursor());
    terminal::show_cursor();
}

void EditorView::redraw_dirty_range(const text::LineEditor & line, bool caps_lock, text::EditorDirtyRange dirty)
{
    switch (dirty.kind)
    {
    case text::EditorDirtyKind::None:
        return;
    case text::EditorDirtyKind::CursorOnly:
        set_cursor(caps_lock, line.cursor());
        terminal::show_cursor();
        return;
    case text::EditorDirtyKind::Full:
        redraw_prompt_and_line(line, caps_lock);
        return;
    case text::EditorDirtyKind::Partial:
        break;
    }

    terminal::hide_cursor();

    const text::EditorViewLayout current_layout = layout(caps_lock);
    const uint64_t rows_needed = current_layout.visual_rows(line.view().size());

    scroll_to_fit(rows_needed);
    draw_text_range(line.view(), current_layout, dirty.start, dirty.new_end);
    clear_text_range(current_layout, dirty.new_end, dirty.old_end);
    mark_line_continuations(max_u64(position_.rendered_rows, rows_needed), rows_needed);
    position_.rendered_rows = rows_needed;

    set_cursor(caps_lock, line.cursor());
    terminal::show_cursor();
}

void EditorView::redraw_change(const text::LineEditor & line, bool caps_lock, text::EditorEditKind edit, text::EditorSnapshot before)
{
    const text::EditorSnapshot after = snapshot(line, caps_lock);
    redraw_dirty_range(line, caps_lock, editor_dirty_range(edit, before, after));
}

void EditorView::write_new_prompt_and_line(const text::LineEditor & line, bool caps_lock)
{
    position_.prompt_column = terminal::cursor_column();
    position_.prompt_row = terminal::cursor_row();
    position_.rendered_rows = 1;
    redraw_prompt_and_line(line, caps_lock);
}

void EditorView::resynchronize_after_terminal_resize(const text::LineEditor & line, bool caps_lock)
{
    const text::EditorViewLayout resized_layout(terminal::columns(),
                                                0,
                                                prompt_for_caps(caps_lock).size());
    position_.prompt_column = 0;
    position_.prompt_row =
        resized_layout.prompt_row_for_terminal_cursor(terminal::cursor_row(), line.cursor());
    position_.rendered_rows = resized_layout.visual_rows(line.size());
    redraw_prompt_and_line(line, caps_lock);
}

void EditorView::move_to_line_end(const text::LineEditor & line, bool caps_lock) const
{
    set_cursor(caps_lock, line.view().size());
}

void EditorView::redraw_history_result(text::HistoryResult result, StringView command, text::LineEditor & line, bool caps_lock)
{
    switch (result)
    {
    case text::HistoryResult::Command:
        if (line.replace(command))
        {
            redraw_prompt_and_line(line, caps_lock);
        }
        break;
    case text::HistoryResult::Blank:
        line.clear();
        redraw_prompt_and_line(line, caps_lock);
        break;
    case text::HistoryResult::None:
        break;
    }
}

} // namespace kernel::shell
