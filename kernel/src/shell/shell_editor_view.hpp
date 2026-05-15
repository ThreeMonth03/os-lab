#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/base/string_view.hpp"
#include "kernel/text/editor_dirty_range.hpp"
#include "kernel/text/editor_view_layout.hpp"
#include "kernel/text/history.hpp"
#include "kernel/text/line_editor.hpp"

namespace kernel::shell
{

class EditorView
{
public:
    [[nodiscard]] text::EditorSnapshot snapshot(const text::LineEditor & line, bool caps_lock) const;

    void redraw_prompt_and_line(const text::LineEditor & line, bool caps_lock);
    void redraw_change(const text::LineEditor & line, bool caps_lock, text::EditorEditKind edit, text::EditorSnapshot before);
    void write_new_prompt_and_line(const text::LineEditor & line, bool caps_lock);
    void move_to_line_end(const text::LineEditor & line, bool caps_lock) const;
    void redraw_history_result(text::HistoryResult result, StringView command, text::LineEditor & line, bool caps_lock);

private:
    struct LinePosition
    {
        uint64_t prompt_column = 0;
        uint64_t prompt_row = 0;
        uint64_t input_column = 0;
        uint64_t input_row = 0;
        uint64_t rendered_rows = 1;
    };

    [[nodiscard]] static StringView prompt_for_caps(bool caps_lock);
    [[nodiscard]] text::EditorViewLayout layout(bool caps_lock) const;

    void scroll_to_fit(uint64_t rows_needed);
    void clear_rendered_area(uint64_t rows_to_clear) const;
    void set_cursor(bool caps_lock, size_t index) const;
    void draw_text_range(StringView text, const text::EditorViewLayout & layout, size_t start, size_t end) const;
    void clear_text_range(const text::EditorViewLayout & layout, size_t start, size_t end) const;
    void redraw_dirty_range(const text::LineEditor & line, bool caps_lock, text::EditorDirtyRange dirty);

    LinePosition position_;
};

} // namespace kernel::shell
