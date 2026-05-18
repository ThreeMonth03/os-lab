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

TEST(TextBufferTest, ResizePreservingVisibleContentKeepsRecentRowsWhenShrinking)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(6, 4));
    ASSERT_TRUE(previous.put(0, 0, 'a'));
    ASSERT_TRUE(previous.put(0, 1, 'b'));
    ASSERT_TRUE(previous.put(0, 2, 'c'));
    ASSERT_TRUE(previous.put(0, 3, 'd'));

    kernel::text::TextBuffer resized;
    const kernel::text::TextBuffer::ResizeResult result =
        resized.resize_preserving_visible_content(previous, 4, 2, 5, 3);

    ASSERT_TRUE(result.resized);
    EXPECT_EQ(result.cursor_column, 3u);
    EXPECT_EQ(result.cursor_row, 1u);
    EXPECT_EQ(resized.glyph_at(0, 0), 'c');
    EXPECT_EQ(resized.glyph_at(0, 1), 'd');
    EXPECT_EQ(resized.glyph_at(4, 0), kernel::text::kTextBufferBlank);
}

TEST(TextBufferTest, ResizePreservingVisibleContentKeepsExistingRowsWhenGrowing)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(3, 2));
    ASSERT_TRUE(previous.put(0, 0, 'a'));
    ASSERT_TRUE(previous.put(2, 1, 'b'));

    kernel::text::TextBuffer resized;
    const kernel::text::TextBuffer::ResizeResult result =
        resized.resize_preserving_visible_content(previous, 5, 4, 2, 1);

    ASSERT_TRUE(result.resized);
    EXPECT_EQ(result.cursor_column, 2u);
    EXPECT_EQ(result.cursor_row, 1u);
    EXPECT_EQ(resized.glyph_at(0, 0), 'a');
    EXPECT_EQ(resized.glyph_at(2, 1), 'b');
    EXPECT_EQ(resized.glyph_at(3, 1), kernel::text::kTextBufferBlank);
    EXPECT_EQ(resized.glyph_at(0, 2), kernel::text::kTextBufferBlank);
}

TEST(TextBufferTest, ResizePreservingVisibleContentRejectsInvalidDimensionsWithoutMutation)
{
    kernel::text::TextBuffer previous;
    ASSERT_TRUE(previous.reset(2, 2));

    kernel::text::TextBuffer resized;
    ASSERT_TRUE(resized.reset(2, 1));
    ASSERT_TRUE(resized.put(0, 0, 'x'));

    const kernel::text::TextBuffer::ResizeResult result =
        resized.resize_preserving_visible_content(previous, 0, 4, 1, 1);

    EXPECT_FALSE(result.resized);
    EXPECT_EQ(resized.columns(), 2u);
    EXPECT_EQ(resized.rows(), 1u);
    EXPECT_EQ(resized.glyph_at(0, 0), 'x');
}

TEST(TextBufferTest, ResizePreservingVisibleContentRejectsSelfSourceWithoutMutation)
{
    kernel::text::TextBuffer buffer;
    ASSERT_TRUE(buffer.reset(2, 1));
    ASSERT_TRUE(buffer.put(0, 0, 'x'));

    const kernel::text::TextBuffer::ResizeResult result =
        buffer.resize_preserving_visible_content(buffer, 4, 2, 0, 0);

    EXPECT_FALSE(result.resized);
    EXPECT_EQ(buffer.columns(), 2u);
    EXPECT_EQ(buffer.rows(), 1u);
    EXPECT_EQ(buffer.glyph_at(0, 0), 'x');
}
