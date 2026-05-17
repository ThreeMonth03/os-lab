#include <gtest/gtest.h>

#include "kernel/display/display_frame.hpp"

TEST(DisplayFrameTest, SubmitsImmediatelyOutsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});

    const kernel::display::DisplayFrameSubmit submit =
        frame.submit({2, 3, 4, 5});

    ASSERT_TRUE(submit.immediate);
    ASSERT_EQ(submit.present_operations.count(), 1u);
    EXPECT_EQ(submit.present_operations.at(0).rect.x, 2u);
    EXPECT_EQ(submit.present_operations.at(0).rect.y, 3u);
    EXPECT_EQ(submit.present_operations.at(0).rect.width, 4u);
    EXPECT_EQ(submit.present_operations.at(0).rect.height, 5u);
}

TEST(DisplayFrameTest, KeepsDisjointPresentRectsSmallInsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({1, 1, 2, 2}).immediate);
    EXPECT_FALSE(frame.submit({70, 90, 4, 4}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_EQ(flush.present_operations.count(), 2u);
    EXPECT_EQ(flush.present_operations.at(0).rect.x, 1u);
    EXPECT_EQ(flush.present_operations.at(0).rect.y, 1u);
    EXPECT_EQ(flush.present_operations.at(0).rect.width, 2u);
    EXPECT_EQ(flush.present_operations.at(0).rect.height, 2u);
    EXPECT_EQ(flush.present_operations.at(1).rect.x, 70u);
    EXPECT_EQ(flush.present_operations.at(1).rect.y, 90u);
    EXPECT_EQ(flush.present_operations.at(1).rect.width, 4u);
    EXPECT_EQ(flush.present_operations.at(1).rect.height, 4u);
}

TEST(DisplayFrameTest, ClipsPresentRectsToFrameBounds)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({70, 90, 20, 20}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_EQ(flush.present_operations.count(), 1u);
    EXPECT_EQ(flush.present_operations.at(0).rect.x, 70u);
    EXPECT_EQ(flush.present_operations.at(0).rect.y, 90u);
    EXPECT_EQ(flush.present_operations.at(0).rect.width, 10u);
    EXPECT_EQ(flush.present_operations.at(0).rect.height, 10u);
}

TEST(DisplayFrameTest, IgnoresEmptyPresentRects)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit(kernel::display::Rect{}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    EXPECT_TRUE(flush.present_operations.empty());
}

TEST(DisplayFrameTest, NestedFrameFlushesOnlyAtOutermostEnd)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();
    frame.begin();

    EXPECT_FALSE(frame.submit({4, 4, 8, 8}).immediate);

    EXPECT_FALSE(frame.end().outermost_frame_ended);
    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    EXPECT_FALSE(flush.present_operations.empty());
    EXPECT_EQ(frame.depth(), 0u);
    EXPECT_TRUE(frame.pending().empty());
}

TEST(DisplayFrameTest, TracksFramePresentStats)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({1, 1, 2, 2}).immediate);
    EXPECT_FALSE(frame.submit({70, 90, 4, 4}).immediate);
    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    const kernel::display::DisplayFrameStats stats = frame.stats();
    EXPECT_EQ(stats.frame_flush_count, 1u);
    EXPECT_EQ(stats.present_rect_count, 2u);
    EXPECT_EQ(stats.total_presented_pixels, 20u);
    EXPECT_EQ(stats.largest_present_rect_area, 16u);
}

TEST(DisplayFrameTest, PreservesScrollOperationInsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    kernel::display::PresentOperationList operations(frame.bounds());
    operations.append_rect({0, 0, 10, 10});
    operations.append_scroll({0, 10, 80, 40}, 8);
    operations.append_rect({0, 42, 80, 8});

    frame.begin();
    EXPECT_FALSE(frame.submit(operations).immediate);
    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_EQ(flush.present_operations.count(), 3u);
    EXPECT_EQ(flush.present_operations.at(0).kind, kernel::display::PresentOperationKind::Rect);
    EXPECT_EQ(flush.present_operations.at(1).kind, kernel::display::PresentOperationKind::Scroll);
    EXPECT_EQ(flush.present_operations.at(1).distance, 8u);
    EXPECT_EQ(flush.present_operations.at(2).kind, kernel::display::PresentOperationKind::Rect);

    const kernel::display::DisplayFrameStats stats = frame.stats();
    EXPECT_EQ(stats.present_operation_count, 3u);
    EXPECT_EQ(stats.present_rect_count, 2u);
    EXPECT_EQ(stats.present_scroll_count, 1u);
}

TEST(DisplayFrameTest, CoalescesRepeatedScrollOperationsInsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    kernel::display::PresentOperationList first(frame.bounds());
    first.append_scroll({0, 10, 80, 40}, 8);
    first.append_scroll_exposed_rect({0, 42, 80, 8});
    EXPECT_FALSE(frame.submit(first).immediate);

    kernel::display::PresentOperationList second(frame.bounds());
    second.append_scroll({0, 10, 80, 40}, 8);
    second.append_scroll_exposed_rect({0, 42, 80, 8});
    EXPECT_FALSE(frame.submit(second).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_EQ(flush.present_operations.count(), 2u);
    EXPECT_EQ(flush.present_operations.at(0).kind, kernel::display::PresentOperationKind::Scroll);
    EXPECT_EQ(flush.present_operations.at(0).distance, 16u);
    EXPECT_TRUE(flush.present_operations.at(1).scroll_exposed_rect_present());
}

TEST(DisplayFrameTest, CompactsComplexRepeatedScrollFrameToSingleRect)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    kernel::display::PresentOperationList first(frame.bounds());
    first.append_scroll({0, 10, 80, 40}, 8);
    EXPECT_FALSE(frame.submit(first).immediate);
    EXPECT_FALSE(frame.submit({0, 42, 8, 8}).immediate);

    kernel::display::PresentOperationList second(frame.bounds());
    second.append_scroll({0, 10, 80, 40}, 8);
    EXPECT_FALSE(frame.submit(second).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_EQ(flush.present_operations.count(), 1u);
    EXPECT_TRUE(flush.present_operations.at(0).normal_rect_present());
    EXPECT_EQ(flush.present_operations.at(0).rect.x, 0u);
    EXPECT_EQ(flush.present_operations.at(0).rect.y, 10u);
    EXPECT_EQ(flush.present_operations.at(0).rect.width, 80u);
    EXPECT_EQ(flush.present_operations.at(0).rect.height, 40u);
}

TEST(DisplayFrameTest, ResetStatsDoesNotClearPendingFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({1, 1, 2, 2}).immediate);
    frame.reset_stats();

    const kernel::display::DisplayFrameFlush flush = frame.end();
    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_EQ(flush.present_operations.count(), 1u);

    const kernel::display::DisplayFrameStats stats = frame.stats();
    EXPECT_EQ(stats.frame_flush_count, 1u);
    EXPECT_EQ(stats.present_rect_count, 1u);
}
