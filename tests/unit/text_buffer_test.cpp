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
