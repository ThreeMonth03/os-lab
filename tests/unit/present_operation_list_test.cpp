#include <gtest/gtest.h>

#include "kernel/display/present_operation_list.hpp"

TEST(PresentOperationListTest, AppendsRectAndScrollInOrder)
{
    kernel::display::PresentOperationList operations({0, 0, 1000, 1000});

    EXPECT_EQ(operations.append_rect({1, 1, 2, 2}),
              kernel::display::PresentOperationAppendResult::Queued);
    EXPECT_EQ(operations.append_scroll({0, 0, 100, 40}, 8),
              kernel::display::PresentOperationAppendResult::Queued);
    EXPECT_EQ(operations.append_rect({0, 40, 100, 8}),
              kernel::display::PresentOperationAppendResult::Queued);

    ASSERT_EQ(operations.count(), 3u);
    EXPECT_EQ(operations.at(0).kind, kernel::display::PresentOperationKind::Rect);
    EXPECT_EQ(operations.at(1).kind, kernel::display::PresentOperationKind::Scroll);
    EXPECT_EQ(operations.at(1).distance, 8u);
    EXPECT_EQ(operations.at(2).kind, kernel::display::PresentOperationKind::Rect);
}

TEST(PresentOperationListTest, MergesOnlyAdjacentRectOperations)
{
    kernel::display::PresentOperationList operations({0, 0, 1000, 1000});

    EXPECT_EQ(operations.append_rect({0, 0, 4, 4}),
              kernel::display::PresentOperationAppendResult::Queued);
    EXPECT_EQ(operations.append_rect({4, 0, 4, 4}),
              kernel::display::PresentOperationAppendResult::Merged);
    EXPECT_EQ(operations.append_scroll({0, 10, 100, 20}, 4),
              kernel::display::PresentOperationAppendResult::Queued);
    EXPECT_EQ(operations.append_rect({8, 0, 4, 4}),
              kernel::display::PresentOperationAppendResult::Queued);

    ASSERT_EQ(operations.count(), 3u);
    EXPECT_EQ(operations.at(0).rect.width, 8u);
    EXPECT_EQ(operations.at(1).kind, kernel::display::PresentOperationKind::Scroll);
    EXPECT_EQ(operations.at(2).rect.x, 8u);
}

TEST(PresentOperationListTest, ClipsOperationsToBounds)
{
    kernel::display::PresentOperationList operations({10, 10, 20, 20});

    EXPECT_EQ(operations.append_rect({25, 25, 10, 10}),
              kernel::display::PresentOperationAppendResult::Queued);
    EXPECT_EQ(operations.append_scroll({5, 5, 10, 10}, 2),
              kernel::display::PresentOperationAppendResult::Queued);

    ASSERT_EQ(operations.count(), 2u);
    EXPECT_EQ(operations.at(0).rect.x, 25u);
    EXPECT_EQ(operations.at(0).rect.y, 25u);
    EXPECT_EQ(operations.at(0).rect.width, 5u);
    EXPECT_EQ(operations.at(0).rect.height, 5u);
    EXPECT_EQ(operations.at(1).rect.x, 10u);
    EXPECT_EQ(operations.at(1).rect.y, 10u);
    EXPECT_EQ(operations.at(1).rect.width, 5u);
    EXPECT_EQ(operations.at(1).rect.height, 5u);
}

TEST(PresentOperationListTest, CapacityFallsBackToFullBounds)
{
    kernel::display::PresentOperationList operations({0, 0, 1000, 1000});

    for (size_t index = 0; index < operations.capacity(); ++index)
    {
        EXPECT_EQ(operations.append_rect({index * 2, 0, 1, 1}),
                  kernel::display::PresentOperationAppendResult::Queued);
    }

    EXPECT_EQ(operations.append_scroll({0, 10, 100, 20}, 4),
              kernel::display::PresentOperationAppendResult::FullscreenFallback);
    ASSERT_EQ(operations.count(), 1u);
    EXPECT_TRUE(operations.full_screen_fallback());
    EXPECT_EQ(operations.at(0).rect.width, 1000u);
    EXPECT_EQ(operations.at(0).rect.height, 1000u);
}

TEST(PresentOperationListTest, TracksOperationStats)
{
    kernel::display::PresentOperationList operations({0, 0, 100, 50});

    operations.append_rect({0, 0, 10, 10});
    operations.append_scroll({0, 10, 20, 10}, 4);

    const kernel::display::PresentOperationStats stats = operations.stats();
    EXPECT_EQ(stats.operation_count, 2u);
    EXPECT_EQ(stats.rect_count, 1u);
    EXPECT_EQ(stats.scroll_count, 1u);
    EXPECT_EQ(stats.total_presented_pixels, 220u);
    EXPECT_EQ(stats.largest_present_rect_area, 100u);
}
