#include "kernel/text/text_buffer.hpp"

#include <gtest/gtest.h>

TEST(TextBufferTest, RejectsInvalidOrOversizedDimensions)
{
    kernel::text::TextBuffer buffer;

    EXPECT_FALSE(buffer.reset(0, 4));
    EXPECT_FALSE(buffer.ready());
    EXPECT_FALSE(buffer.reset(4, 0));
    EXPECT_FALSE(buffer.ready());
    EXPECT_FALSE(buffer.reset(buffer.max_columns() + 1, 4));
    EXPECT_FALSE(buffer.ready());
    EXPECT_FALSE(buffer.reset(4, buffer.max_rows() + 1));
    EXPECT_FALSE(buffer.ready());
}

TEST(TextBufferTest, StoresAndClearsCells)
{
    kernel::text::TextBuffer buffer;

    ASSERT_TRUE(buffer.reset(4, 2));
    EXPECT_TRUE(buffer.put(1, 0, 'A'));
    EXPECT_EQ(buffer.glyph_at(1, 0), 'A');
    EXPECT_FALSE(buffer.put(4, 0, 'B'));
    EXPECT_EQ(buffer.glyph_at(4, 0), kernel::text::kTextBufferBlank);

    EXPECT_TRUE(buffer.clear_cell(1, 0));
    EXPECT_EQ(buffer.glyph_at(1, 0), kernel::text::kTextBufferBlank);

    EXPECT_TRUE(buffer.put(2, 1, 'C'));
    buffer.clear();
    EXPECT_EQ(buffer.glyph_at(2, 1), kernel::text::kTextBufferBlank);
}

TEST(TextBufferTest, ScrollUpMovesTextAndClearsBottomRow)
{
    kernel::text::TextBuffer buffer;

    ASSERT_TRUE(buffer.reset(3, 3));
    ASSERT_TRUE(buffer.put(0, 0, 'a'));
    ASSERT_TRUE(buffer.put(1, 1, 'b'));
    ASSERT_TRUE(buffer.put(2, 2, 'c'));

    ASSERT_TRUE(buffer.scroll_up());
    EXPECT_EQ(buffer.glyph_at(0, 0), kernel::text::kTextBufferBlank);
    EXPECT_EQ(buffer.glyph_at(1, 0), 'b');
    EXPECT_EQ(buffer.glyph_at(2, 1), 'c');
    EXPECT_EQ(buffer.glyph_at(2, 2), kernel::text::kTextBufferBlank);
}

TEST(TextBufferTest, ScrollOnlyTracksTerminalOwnedCells)
{
    kernel::text::TextBuffer buffer;

    ASSERT_TRUE(buffer.reset(2, 2));
    ASSERT_TRUE(buffer.put(0, 1, 'x'));

    ASSERT_TRUE(buffer.scroll_up());
    EXPECT_EQ(buffer.glyph_at(0, 0), 'x');
    EXPECT_EQ(buffer.glyph_at(1, 0), kernel::text::kTextBufferBlank);
    EXPECT_EQ(buffer.glyph_at(0, 1), kernel::text::kTextBufferBlank);
    EXPECT_EQ(buffer.glyph_at(1, 1), kernel::text::kTextBufferBlank);
}

TEST(TextBufferTest, ReflowingResizeWrapsLongVisibleLineWhenShrinking)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(6, 4));
    ASSERT_TRUE(previous.put(0, 0, 'a'));
    ASSERT_TRUE(previous.put(1, 0, 'b'));
    ASSERT_TRUE(previous.put(2, 0, 'c'));
    ASSERT_TRUE(previous.put(3, 0, 'd'));
    ASSERT_TRUE(previous.put(4, 0, 'e'));

    kernel::text::TextBuffer resized;
    const kernel::text::TextBuffer::ResizeResult result =
        resized.resize_reflowing_visible_content(previous, 3, 3, 5, 0);

    ASSERT_TRUE(result.resized);
    EXPECT_EQ(result.cursor_column, 2u);
    EXPECT_EQ(result.cursor_row, 1u);
    EXPECT_EQ(resized.glyph_at(0, 0), 'a');
    EXPECT_EQ(resized.glyph_at(1, 0), 'b');
    EXPECT_EQ(resized.glyph_at(2, 0), 'c');
    EXPECT_EQ(resized.glyph_at(0, 1), 'd');
    EXPECT_EQ(resized.glyph_at(1, 1), 'e');
    EXPECT_TRUE(resized.row_continues_from_previous(1));
}

TEST(TextBufferTest, ReflowingResizeDoesNotConcatHardLines)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(4, 3));
    ASSERT_TRUE(previous.put(0, 0, 'a'));
    ASSERT_TRUE(previous.put(1, 0, 'b'));
    ASSERT_TRUE(previous.put(0, 1, 'c'));
    ASSERT_TRUE(previous.put(1, 1, 'd'));

    kernel::text::TextBuffer resized;
    const kernel::text::TextBuffer::ResizeResult result =
        resized.resize_reflowing_visible_content(previous, 6, 3, 2, 1);

    ASSERT_TRUE(result.resized);
    EXPECT_EQ(result.cursor_column, 2u);
    EXPECT_EQ(result.cursor_row, 1u);
    EXPECT_EQ(resized.glyph_at(0, 0), 'a');
    EXPECT_EQ(resized.glyph_at(1, 0), 'b');
    EXPECT_EQ(resized.glyph_at(2, 0), kernel::text::kTextBufferBlank);
    EXPECT_EQ(resized.glyph_at(0, 1), 'c');
    EXPECT_EQ(resized.glyph_at(1, 1), 'd');
    EXPECT_FALSE(resized.row_continues_from_previous(1));
}

TEST(TextBufferTest, ReflowingResizeCanWidenSoftWrappedRows)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(3, 3));
    ASSERT_TRUE(previous.put(0, 0, 'a'));
    ASSERT_TRUE(previous.put(1, 0, 'b'));
    ASSERT_TRUE(previous.put(2, 0, 'c'));
    ASSERT_TRUE(previous.put(0, 1, 'd'));
    ASSERT_TRUE(previous.put(1, 1, 'e'));
    ASSERT_TRUE(previous.set_row_continuation(1, true));

    kernel::text::TextBuffer resized;
    const kernel::text::TextBuffer::ResizeResult result =
        resized.resize_reflowing_visible_content(previous, 6, 2, 2, 1);

    ASSERT_TRUE(result.resized);
    EXPECT_EQ(result.cursor_column, 5u);
    EXPECT_EQ(result.cursor_row, 0u);
    EXPECT_EQ(resized.glyph_at(0, 0), 'a');
    EXPECT_EQ(resized.glyph_at(1, 0), 'b');
    EXPECT_EQ(resized.glyph_at(2, 0), 'c');
    EXPECT_EQ(resized.glyph_at(3, 0), 'd');
    EXPECT_EQ(resized.glyph_at(4, 0), 'e');
    EXPECT_FALSE(resized.row_continues_from_previous(1));
}

TEST(TextBufferTest, ReflowingResizeNarrowThenWidePreservesHardLines)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(8, 3));
    ASSERT_TRUE(previous.put(0, 0, 'a'));
    ASSERT_TRUE(previous.put(1, 0, 'b'));
    ASSERT_TRUE(previous.put(2, 0, 'c'));
    ASSERT_TRUE(previous.put(3, 0, 'd'));
    ASSERT_TRUE(previous.put(4, 0, 'e'));
    ASSERT_TRUE(previous.put(5, 0, 'f'));
    ASSERT_TRUE(previous.put(0, 1, 'x'));
    ASSERT_TRUE(previous.put(1, 1, 'y'));

    kernel::text::TextBuffer narrow;
    ASSERT_TRUE(narrow.resize_reflowing_visible_content(previous, 4, 3, 2, 1).resized);
    EXPECT_TRUE(narrow.row_continues_from_previous(1));
    EXPECT_FALSE(narrow.row_continues_from_previous(2));

    kernel::text::TextBuffer wide;
    const kernel::text::TextBuffer::ResizeResult result =
        wide.resize_reflowing_visible_content(narrow, 8, 3, 2, 2);

    ASSERT_TRUE(result.resized);
    EXPECT_EQ(wide.glyph_at(0, 0), 'a');
    EXPECT_EQ(wide.glyph_at(1, 0), 'b');
    EXPECT_EQ(wide.glyph_at(2, 0), 'c');
    EXPECT_EQ(wide.glyph_at(3, 0), 'd');
    EXPECT_EQ(wide.glyph_at(4, 0), 'e');
    EXPECT_EQ(wide.glyph_at(5, 0), 'f');
    EXPECT_EQ(wide.glyph_at(6, 0), kernel::text::kTextBufferBlank);
    EXPECT_EQ(wide.glyph_at(0, 1), 'x');
    EXPECT_EQ(wide.glyph_at(1, 1), 'y');
    EXPECT_FALSE(wide.row_continues_from_previous(1));
}

TEST(TextBufferTest, ReflowingResizeKeepsRecentRowsWhenShrinkingHeight)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(4, 4));
    ASSERT_TRUE(previous.put(0, 0, 'a'));
    ASSERT_TRUE(previous.put(0, 1, 'b'));
    ASSERT_TRUE(previous.put(0, 2, 'c'));
    ASSERT_TRUE(previous.put(0, 3, 'd'));

    kernel::text::TextBuffer resized;
    const kernel::text::TextBuffer::ResizeResult result =
        resized.resize_reflowing_visible_content(previous, 4, 2, 1, 3);

    ASSERT_TRUE(result.resized);
    EXPECT_EQ(result.cursor_column, 1u);
    EXPECT_EQ(result.cursor_row, 1u);
    EXPECT_EQ(resized.glyph_at(0, 0), 'c');
    EXPECT_EQ(resized.glyph_at(0, 1), 'd');
}

TEST(TextBufferTest, ReflowingResizeHeightShrinkThenGrowKeepsCursorWithCommandLine)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(4, 4));
    ASSERT_TRUE(previous.put(0, 1, '>'));
    ASSERT_TRUE(previous.put(1, 1, ' '));
    ASSERT_TRUE(previous.put(2, 1, 'a'));
    ASSERT_TRUE(previous.put(3, 1, 'b'));
    ASSERT_TRUE(previous.put(0, 2, 'c'));
    ASSERT_TRUE(previous.put(1, 2, 'd'));
    ASSERT_TRUE(previous.set_row_continuation(2, true));

    kernel::text::TextBuffer shorter;
    const kernel::text::TextBuffer::ResizeResult shorter_result =
        shorter.resize_reflowing_visible_content(previous, 4, 2, 2, 2);

    ASSERT_TRUE(shorter_result.resized);
    EXPECT_EQ(shorter_result.cursor_column, 2u);
    EXPECT_EQ(shorter_result.cursor_row, 1u);
    EXPECT_EQ(shorter.glyph_at(0, 0), '>');
    EXPECT_EQ(shorter.glyph_at(1, 0), ' ');
    EXPECT_EQ(shorter.glyph_at(2, 0), 'a');
    EXPECT_EQ(shorter.glyph_at(3, 0), 'b');
    EXPECT_EQ(shorter.glyph_at(0, 1), 'c');
    EXPECT_EQ(shorter.glyph_at(1, 1), 'd');
    EXPECT_TRUE(shorter.row_continues_from_previous(1));

    kernel::text::TextBuffer taller;
    const kernel::text::TextBuffer::ResizeResult taller_result =
        taller.resize_reflowing_visible_content(shorter,
                                                4,
                                                4,
                                                shorter_result.cursor_column,
                                                shorter_result.cursor_row);

    ASSERT_TRUE(taller_result.resized);
    EXPECT_EQ(taller_result.cursor_column, 2u);
    EXPECT_EQ(taller_result.cursor_row, 1u);
    EXPECT_EQ(taller.glyph_at(0, 0), '>');
    EXPECT_EQ(taller.glyph_at(0, 1), 'c');
    EXPECT_EQ(taller.glyph_at(1, 1), 'd');
    EXPECT_TRUE(taller.row_continues_from_previous(1));
}

TEST(TextBufferTest, ReflowingResizeKeepsExistingRowsWhenGrowingHeight)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(3, 2));
    ASSERT_TRUE(previous.put(0, 0, 'a'));
    ASSERT_TRUE(previous.put(2, 1, 'b'));

    kernel::text::TextBuffer resized;
    const kernel::text::TextBuffer::ResizeResult result =
        resized.resize_reflowing_visible_content(previous, 3, 4, 2, 1);

    ASSERT_TRUE(result.resized);
    EXPECT_EQ(result.cursor_column, 2u);
    EXPECT_EQ(result.cursor_row, 1u);
    EXPECT_EQ(resized.glyph_at(0, 0), 'a');
    EXPECT_EQ(resized.glyph_at(2, 1), 'b');
    EXPECT_EQ(resized.glyph_at(0, 2), kernel::text::kTextBufferBlank);
}

TEST(TextBufferTest, ReflowingResizeRejectsInvalidDimensionsWithoutMutation)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(2, 2));

    kernel::text::TextBuffer resized;
    ASSERT_TRUE(resized.reset(2, 1));
    ASSERT_TRUE(resized.put(0, 0, 'x'));

    const kernel::text::TextBuffer::ResizeResult result =
        resized.resize_reflowing_visible_content(previous, 0, 4, 1, 1);

    EXPECT_FALSE(result.resized);
    EXPECT_EQ(resized.columns(), 2u);
    EXPECT_EQ(resized.rows(), 1u);
    EXPECT_EQ(resized.glyph_at(0, 0), 'x');
}

TEST(TextBufferTest, ReflowingResizeRejectsSelfSourceWithoutMutation)
{
    kernel::text::TextBuffer buffer;
    ASSERT_TRUE(buffer.reset(2, 1));
    ASSERT_TRUE(buffer.put(0, 0, 'x'));

    const kernel::text::TextBuffer::ResizeResult result =
        buffer.resize_reflowing_visible_content(buffer, 4, 2, 0, 0);

    EXPECT_FALSE(result.resized);
    EXPECT_EQ(buffer.columns(), 2u);
    EXPECT_EQ(buffer.rows(), 1u);
    EXPECT_EQ(buffer.glyph_at(0, 0), 'x');
}
