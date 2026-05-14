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
    [[nodiscard]] EditorSnapshot snapshot(const LineEditor & line, bool caps_lock) const;

    void redraw_prompt_and_line(const LineEditor & line, bool caps_lock);
    void redraw_change(const LineEditor & line, bool caps_lock, EditorEditKind edit, EditorSnapshot before);
    void write_new_prompt_and_line(const LineEditor & line, bool caps_lock);
    void move_to_line_end(const LineEditor & line, bool caps_lock) const;
    void redraw_history_result(HistoryResult result, StringView command, LineEditor & line, bool caps_lock);

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
    [[nodiscard]] EditorViewLayout layout(bool caps_lock) const;

    void scroll_to_fit(uint64_t rows_needed);
    void clear_rendered_area(uint64_t rows_to_clear) const;
    void set_cursor(bool caps_lock, size_t index) const;
    void draw_text_range(StringView text, const EditorViewLayout & layout, size_t start, size_t end) const;
    void clear_text_range(const EditorViewLayout & layout, size_t start, size_t end) const;
    void redraw_dirty_range(const LineEditor & line, bool caps_lock, EditorDirtyRange dirty);

    LinePosition position_;
};

} // namespace kernel::shell
