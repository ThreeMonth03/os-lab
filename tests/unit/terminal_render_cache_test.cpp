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
