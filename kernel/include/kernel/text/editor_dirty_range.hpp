#pragma once

#include <stddef.h>

namespace kernel
{

enum class EditorEditKind
{
    CursorMove,
    Insert,
    Backspace,
    DeleteForward,
    ReplaceAll,
    PromptChange,
};

enum class EditorDirtyKind
{
    None,
    CursorOnly,
    Partial,
    Full,
};

struct EditorSnapshot
{
    size_t cursor = 0;
    size_t size = 0;
    size_t prompt_width = 0;
};

struct EditorDirtyRange
{
    EditorDirtyKind kind = EditorDirtyKind::None;
    size_t start = 0;
    size_t old_end = 0;
    size_t new_end = 0;
};

[[nodiscard]] EditorDirtyRange editor_dirty_range(EditorEditKind edit, EditorSnapshot before, EditorSnapshot after);

} // namespace kernel
