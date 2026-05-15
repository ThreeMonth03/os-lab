#include "kernel/text/editor_dirty_range.hpp"

namespace kernel::text
{

namespace
{

EditorDirtyRange full_redraw() { return {EditorDirtyKind::Full, 0, 0, 0}; }

EditorDirtyRange partial_redraw(size_t start, size_t old_end, size_t new_end)
{
    return {EditorDirtyKind::Partial, start, old_end, new_end};
}

} // namespace

EditorDirtyRange editor_dirty_range(EditorEditKind edit, EditorSnapshot before, EditorSnapshot after)
{
    if (before.prompt_width != after.prompt_width)
    {
        return full_redraw();
    }

    switch (edit)
    {
    case EditorEditKind::CursorMove:
        if (before.cursor == after.cursor && before.size == after.size)
        {
            return {};
        }
        if (before.size == after.size)
        {
            return {EditorDirtyKind::CursorOnly, after.cursor, before.size, after.size};
        }
        return full_redraw();
    case EditorEditKind::Insert:
        if (after.size == before.size + 1 && after.cursor == before.cursor + 1)
        {
            return partial_redraw(before.cursor, before.size, after.size);
        }
        return full_redraw();
    case EditorEditKind::Backspace:
        if (before.cursor > 0 && after.cursor + 1 == before.cursor &&
            after.size + 1 == before.size)
        {
            return partial_redraw(after.cursor, before.size, after.size);
        }
        return full_redraw();
    case EditorEditKind::DeleteForward:
        if (after.cursor == before.cursor && after.size + 1 == before.size)
        {
            return partial_redraw(before.cursor, before.size, after.size);
        }
        return full_redraw();
    case EditorEditKind::ReplaceAll:
    case EditorEditKind::PromptChange:
        return full_redraw();
    }

    return full_redraw();
}

} // namespace kernel::text
