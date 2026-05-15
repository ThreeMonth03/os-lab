#include <stddef.h>
#include <stdint.h>
#include <gtest/gtest.h>
#include "kernel/text/editor_dirty_range.hpp"
#include "kernel/text/editor_view_layout.hpp"
#include "kernel/text/history.hpp"
#include "kernel/text/line_editor.hpp"
#include "test_helpers.hpp"

namespace
{

using os_lab::test::expect_text;

void expect_editor_cell(kernel::text::EditorViewCell actual, uint64_t column, uint64_t row)
{
    EXPECT_EQ(actual.column, column);
    EXPECT_EQ(actual.row, row);
}

void expect_dirty_range(kernel::text::EditorDirtyRange actual, kernel::text::EditorDirtyKind kind, size_t start, size_t old_end, size_t new_end)
{
    EXPECT_EQ(actual.kind, kind);
    EXPECT_EQ(actual.start, start);
    EXPECT_EQ(actual.old_end, old_end);
    EXPECT_EQ(actual.new_end, new_end);
}

TEST(LineEditorTest, InsertsMovesCursorAndDeletes)
{
    kernel::text::LineEditor line;

    EXPECT_TRUE(line.empty());
    EXPECT_EQ(line.cursor(), 0u);
    EXPECT_FALSE(line.move_left());

    EXPECT_TRUE(line.insert('a'));
    EXPECT_TRUE(line.insert('b'));
    EXPECT_TRUE(line.insert('c'));
    expect_text(line.view(), "abc");
    EXPECT_EQ(line.cursor(), 3u);
    EXPECT_FALSE(line.move_right());

    EXPECT_TRUE(line.move_left());
    EXPECT_TRUE(line.move_left());
    EXPECT_EQ(line.cursor(), 1u);
    EXPECT_TRUE(line.insert('X'));
    expect_text(line.view(), "aXbc");
    EXPECT_EQ(line.cursor(), 2u);

    EXPECT_TRUE(line.backspace());
    expect_text(line.view(), "abc");
    EXPECT_EQ(line.cursor(), 1u);

    EXPECT_TRUE(line.delete_forward());
    expect_text(line.view(), "ac");
    EXPECT_EQ(line.cursor(), 1u);

    EXPECT_TRUE(line.move_to_start());
    EXPECT_EQ(line.cursor(), 0u);
    EXPECT_TRUE(line.delete_forward());
    expect_text(line.view(), "c");
    EXPECT_TRUE(line.move_to_end());
    EXPECT_EQ(line.cursor(), 1u);
    EXPECT_TRUE(line.insert('!'));
    expect_text(line.view(), "c!");

    line.clear();
    EXPECT_TRUE(line.empty());
    EXPECT_EQ(line.cursor(), 0u);
}

TEST(LineEditorTest, ReplacesCurrentLine)
{
    kernel::text::LineEditor line;

    EXPECT_TRUE(line.replace("hello"));
    expect_text(line.view(), "hello");
    EXPECT_EQ(line.cursor(), line.size());

    EXPECT_TRUE(line.move_left());
    EXPECT_TRUE(line.replace("z"));
    expect_text(line.view(), "z");
    EXPECT_EQ(line.cursor(), 1u);

    char too_long[kernel::text::LineEditor::capacity + 1] = {};
    for (char & character : too_long)
    {
        character = 'x';
    }

    EXPECT_FALSE(line.replace(kernel::StringView(too_long, kernel::text::LineEditor::capacity + 1)));
    expect_text(line.view(), "z");
}

TEST(LineEditorTest, ComputesTabStopSpacing)
{
    EXPECT_EQ(kernel::text::LineEditor::spaces_to_next_tab_stop(0), 4u);
    EXPECT_EQ(kernel::text::LineEditor::spaces_to_next_tab_stop(1), 3u);
    EXPECT_EQ(kernel::text::LineEditor::spaces_to_next_tab_stop(3), 1u);
    EXPECT_EQ(kernel::text::LineEditor::spaces_to_next_tab_stop(4), 4u);
}

TEST(LineEditorTest, InsertsSpacesAtCursor)
{
    kernel::text::LineEditor line;

    EXPECT_TRUE(line.insert('a'));
    EXPECT_TRUE(line.insert('b'));
    EXPECT_TRUE(line.move_left());
    EXPECT_TRUE(line.insert_spaces(kernel::text::LineEditor::spaces_to_next_tab_stop(line.cursor())));

    expect_text(line.view(), "a   b");
    EXPECT_EQ(line.cursor(), 4u);
}

TEST(HistoryTest, BrowsesCommandsAndReturnsBlankAtNewest)
{
    kernel::text::History history;
    kernel::StringView command;

    EXPECT_TRUE(history.empty());
    EXPECT_EQ(history.previous(command), kernel::text::HistoryResult::None);
    EXPECT_EQ(history.next(command), kernel::text::HistoryResult::None);
    EXPECT_FALSE(history.push(""));

    EXPECT_TRUE(history.push("one"));
    EXPECT_TRUE(history.push("two"));
    EXPECT_EQ(history.size(), 2u);

    EXPECT_EQ(history.previous(command), kernel::text::HistoryResult::Command);
    expect_text(command, "two");
    EXPECT_EQ(history.previous(command), kernel::text::HistoryResult::Command);
    expect_text(command, "one");
    EXPECT_EQ(history.previous(command), kernel::text::HistoryResult::Command);
    expect_text(command, "one");

    EXPECT_EQ(history.next(command), kernel::text::HistoryResult::Command);
    expect_text(command, "two");
    EXPECT_EQ(history.next(command), kernel::text::HistoryResult::Blank);
    EXPECT_TRUE(command.empty());
    EXPECT_EQ(history.next(command), kernel::text::HistoryResult::None);

    EXPECT_EQ(history.previous(command), kernel::text::HistoryResult::Command);
    expect_text(command, "two");
    history.reset_browse();
    EXPECT_EQ(history.next(command), kernel::text::HistoryResult::None);
}

TEST(HistoryTest, KeepsFixedCapacity)
{
    kernel::text::History history;
    kernel::StringView command;

    for (size_t index = 0; index < kernel::text::History::capacity + 1; ++index)
    {
        const char value[1] = {static_cast<char>('0' + index)};
        EXPECT_TRUE(history.push({value, 1}));
    }

    EXPECT_EQ(history.size(), kernel::text::History::capacity);
    EXPECT_EQ(history.previous(command), kernel::text::HistoryResult::Command);
    expect_text(command, "8");

    for (size_t index = 1; index < kernel::text::History::capacity; ++index)
    {
        EXPECT_EQ(history.previous(command), kernel::text::HistoryResult::Command);
    }
    expect_text(command, "1");
}

TEST(HistoryTest, StoresFullLineEditorCapacityCommands)
{
    kernel::text::History history;
    kernel::StringView command;
    char text[kernel::text::LineEditor::capacity] = {};

    for (char & character : text)
    {
        character = 'x';
    }

    EXPECT_TRUE(history.push({text, kernel::text::LineEditor::capacity}));
    EXPECT_EQ(history.previous(command), kernel::text::HistoryResult::Command);
    EXPECT_EQ(command.size(), kernel::text::LineEditor::capacity);
}

TEST(EditorViewLayoutTest, MapsCursorWithinFirstVisualRow)
{
    const kernel::text::EditorViewLayout layout(10, 0, 2);

    expect_editor_cell(layout.position_for(0), 2, 0);
    expect_editor_cell(layout.position_for(7), 9, 0);
    EXPECT_EQ(layout.visual_rows(7), 1u);
}

TEST(EditorViewLayoutTest, WrapsCursorAfterRowEnd)
{
    const kernel::text::EditorViewLayout layout(10, 0, 2);

    expect_editor_cell(layout.position_for(8), 0, 1);
    expect_editor_cell(layout.position_for(18), 0, 2);
    EXPECT_EQ(layout.visual_rows(8), 2u);
    EXPECT_EQ(layout.visual_rows(18), 3u);
}

TEST(EditorViewLayoutTest, AccountsForPromptColumnOffset)
{
    const kernel::text::EditorViewLayout layout(10, 5, 2);

    expect_editor_cell(layout.position_for(0), 7, 0);
    expect_editor_cell(layout.position_for(2), 9, 0);
    expect_editor_cell(layout.position_for(3), 0, 1);
    EXPECT_EQ(layout.visual_rows(3), 2u);
}

TEST(EditorViewLayoutTest, SupportsLongPrompt)
{
    const kernel::text::EditorViewLayout layout(12, 0, 9);

    expect_editor_cell(layout.position_for(0), 9, 0);
    expect_editor_cell(layout.position_for(3), 0, 1);
    expect_editor_cell(layout.position_for(15), 0, 2);
}

TEST(EditorViewLayoutTest, HandlesZeroColumns)
{
    const kernel::text::EditorViewLayout layout(0, 4, 2);

    EXPECT_FALSE(layout.ready());
    expect_editor_cell(layout.position_for(10), 0, 0);
    EXPECT_EQ(layout.visual_rows(10), 1u);
}

TEST(EditorDirtyRangeTest, AppendAtEndRedrawsOnlyNewCharacter)
{
    const kernel::text::EditorDirtyRange dirty =
        kernel::text::editor_dirty_range(kernel::text::EditorEditKind::Insert, {5, 5, 2}, {6, 6, 2});

    expect_dirty_range(dirty, kernel::text::EditorDirtyKind::Partial, 5, 5, 6);
}

TEST(EditorDirtyRangeTest, InsertInMiddleRedrawsFromInsertionPoint)
{
    const kernel::text::EditorDirtyRange dirty =
        kernel::text::editor_dirty_range(kernel::text::EditorEditKind::Insert, {2, 5, 2}, {3, 6, 2});

    expect_dirty_range(dirty, kernel::text::EditorDirtyKind::Partial, 2, 5, 6);
}

TEST(EditorDirtyRangeTest, BackspaceRedrawsFromRemovedCharacter)
{
    const kernel::text::EditorDirtyRange dirty =
        kernel::text::editor_dirty_range(kernel::text::EditorEditKind::Backspace, {3, 5, 2}, {2, 4, 2});

    expect_dirty_range(dirty, kernel::text::EditorDirtyKind::Partial, 2, 5, 4);
}

TEST(EditorDirtyRangeTest, DeleteRedrawsFromCursor)
{
    const kernel::text::EditorDirtyRange dirty =
        kernel::text::editor_dirty_range(kernel::text::EditorEditKind::DeleteForward, {2, 5, 2}, {2, 4, 2});

    expect_dirty_range(dirty, kernel::text::EditorDirtyKind::Partial, 2, 5, 4);
}

TEST(EditorDirtyRangeTest, CursorMoveDoesNotRedrawText)
{
    const kernel::text::EditorDirtyRange dirty =
        kernel::text::editor_dirty_range(kernel::text::EditorEditKind::CursorMove, {4, 5, 2}, {3, 5, 2});

    expect_dirty_range(dirty, kernel::text::EditorDirtyKind::CursorOnly, 3, 5, 5);
}

TEST(EditorDirtyRangeTest, PromptWidthChangeRequiresFullRedraw)
{
    const kernel::text::EditorDirtyRange dirty =
        kernel::text::editor_dirty_range(kernel::text::EditorEditKind::Insert, {5, 5, 2}, {6, 6, 9});

    expect_dirty_range(dirty, kernel::text::EditorDirtyKind::Full, 0, 0, 0);
}

TEST(EditorDirtyRangeTest, MultilineRangeCanCrossVisualRows)
{
    const kernel::text::EditorViewLayout layout(10, 0, 2);
    const kernel::text::EditorDirtyRange dirty =
        kernel::text::editor_dirty_range(kernel::text::EditorEditKind::Insert, {7, 9, 2}, {8, 10, 2});

    expect_dirty_range(dirty, kernel::text::EditorDirtyKind::Partial, 7, 9, 10);
    expect_editor_cell(layout.position_for(dirty.start), 9, 0);
    expect_editor_cell(layout.position_for(dirty.new_end), 2, 1);
}

} // namespace
