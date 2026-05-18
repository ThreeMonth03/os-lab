#include <gtest/gtest.h>

#include "kernel/display/terminal_render_cache.hpp"
#include "kernel/text/text_buffer.hpp"

TEST(TerminalRenderCacheTest, ResetStartsInvalid)
{
    kernel::display::TerminalRenderCache cache;

    ASSERT_TRUE(cache.reset(4, 2));

    EXPECT_TRUE(cache.ready());
    EXPECT_FALSE(cache.valid());
}

TEST(TerminalRenderCacheTest, CountsOnlyChangedCells)
{
    kernel::text::TextBuffer logical;
    kernel::display::TerminalRenderCache cache;
    ASSERT_TRUE(logical.reset(4, 2));
    ASSERT_TRUE(cache.reset(4, 2));

    ASSERT_TRUE(logical.put(1, 0, 'A'));
    cache.synchronize_from(logical);
    EXPECT_EQ(cache.count_dirty_cells(logical), 0u);

    ASSERT_TRUE(logical.put(1, 0, 'B'));
    ASSERT_TRUE(logical.put(2, 1, 'C'));

    EXPECT_TRUE(cache.needs_render(logical, 1, 0));
    EXPECT_TRUE(cache.needs_render(logical, 2, 1));
    EXPECT_FALSE(cache.needs_render(logical, 0, 0));
    EXPECT_EQ(cache.count_dirty_cells(logical), 2u);
}

TEST(TerminalRenderCacheTest, UnchangedCellsDoNotRequireRendering)
{
    kernel::text::TextBuffer logical;
    kernel::display::TerminalRenderCache cache;
    ASSERT_TRUE(logical.reset(3, 1));
    ASSERT_TRUE(cache.reset(3, 1));
    ASSERT_TRUE(logical.put(0, 0, 'x'));
    cache.synchronize_from(logical);

    EXPECT_FALSE(cache.needs_render(logical, 0, 0));
    EXPECT_FALSE(cache.needs_render(logical, 1, 0));
    EXPECT_EQ(cache.count_dirty_cells(logical), 0u);
}

TEST(TerminalRenderCacheTest, ClearRenderedResetsCacheToBlank)
{
    kernel::text::TextBuffer logical;
    kernel::display::TerminalRenderCache cache;
    ASSERT_TRUE(logical.reset(2, 1));
    ASSERT_TRUE(cache.reset(2, 1));
    ASSERT_TRUE(logical.put(0, 0, 'x'));
    cache.synchronize_from(logical);

    cache.clear_rendered();
    EXPECT_TRUE(cache.valid());
    EXPECT_EQ(cache.glyph_at(0, 0), kernel::text::kTextBufferBlank);
    EXPECT_EQ(cache.count_dirty_cells(logical), 1u);

    ASSERT_TRUE(logical.clear_cell(0, 0));
    EXPECT_EQ(cache.count_dirty_cells(logical), 0u);
}

TEST(TerminalRenderCacheTest, ScrollUpMovesRenderedRowsAndBlanksExposedRows)
{
    kernel::display::TerminalRenderCache cache;
    ASSERT_TRUE(cache.reset(3, 3));
    cache.clear_rendered();
    ASSERT_TRUE(cache.mark_rendered(0, 0, 'a'));
    ASSERT_TRUE(cache.mark_rendered(1, 0, 'b'));
    ASSERT_TRUE(cache.mark_rendered(0, 1, 'c'));
    ASSERT_TRUE(cache.mark_rendered(0, 2, 'd'));

    cache.scroll_up(1);

    EXPECT_EQ(cache.glyph_at(0, 0), 'c');
    EXPECT_EQ(cache.glyph_at(1, 0), kernel::text::kTextBufferBlank);
    EXPECT_EQ(cache.glyph_at(0, 1), 'd');
    EXPECT_EQ(cache.glyph_at(0, 2), kernel::text::kTextBufferBlank);
}

TEST(TerminalRenderCacheTest, ScrollUpStillDetectsUnrenderedFinalCells)
{
    kernel::text::TextBuffer logical;
    kernel::display::TerminalRenderCache cache;
    ASSERT_TRUE(logical.reset(3, 3));
    ASSERT_TRUE(cache.reset(3, 3));
    cache.clear_rendered();

    ASSERT_TRUE(logical.put(0, 2, 'x'));
    ASSERT_TRUE(logical.scroll_up());
    ASSERT_TRUE(logical.scroll_up());
    cache.scroll_up(2);

    EXPECT_TRUE(cache.needs_render(logical, 0, 0));
    EXPECT_EQ(cache.count_dirty_cells(logical), 1u);
}
