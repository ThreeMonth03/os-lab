#include <gtest/gtest.h>

#include "kernel/display/display_frame.hpp"

TEST(DisplayFrameTest, SubmitsImmediatelyOutsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});

    const kernel::display::DisplayFrameSubmit submit =
        frame.submit({2, 3, 4, 5});

    ASSERT_TRUE(submit.immediate);
    ASSERT_EQ(submit.present_regions.count(), 1u);
    EXPECT_EQ(submit.present_regions.at(0).x, 2u);
    EXPECT_EQ(submit.present_regions.at(0).y, 3u);
    EXPECT_EQ(submit.present_regions.at(0).width, 4u);
    EXPECT_EQ(submit.present_regions.at(0).height, 5u);
}

TEST(DisplayFrameTest, KeepsDisjointPresentRectsSmallInsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({1, 1, 2, 2}).immediate);
    EXPECT_FALSE(frame.submit({70, 90, 4, 4}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_EQ(flush.present_regions.count(), 2u);
    EXPECT_EQ(flush.present_regions.at(0).x, 1u);
    EXPECT_EQ(flush.present_regions.at(0).y, 1u);
    EXPECT_EQ(flush.present_regions.at(0).width, 2u);
    EXPECT_EQ(flush.present_regions.at(0).height, 2u);
    EXPECT_EQ(flush.present_regions.at(1).x, 70u);
    EXPECT_EQ(flush.present_regions.at(1).y, 90u);
    EXPECT_EQ(flush.present_regions.at(1).width, 4u);
    EXPECT_EQ(flush.present_regions.at(1).height, 4u);
}

TEST(DisplayFrameTest, ClipsPresentRectsToFrameBounds)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({70, 90, 20, 20}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_EQ(flush.present_regions.count(), 1u);
    EXPECT_EQ(flush.present_regions.at(0).x, 70u);
    EXPECT_EQ(flush.present_regions.at(0).y, 90u);
    EXPECT_EQ(flush.present_regions.at(0).width, 10u);
    EXPECT_EQ(flush.present_regions.at(0).height, 10u);
}

TEST(DisplayFrameTest, IgnoresEmptyPresentRects)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit(kernel::display::Rect{}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    EXPECT_TRUE(flush.present_regions.empty());
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
    EXPECT_FALSE(flush.present_regions.empty());
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
