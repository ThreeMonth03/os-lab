#include <gtest/gtest.h>

#include "kernel/display/display_frame.hpp"

TEST(DisplayFrameTest, SubmitsImmediatelyOutsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});

    const kernel::display::DisplayFrameSubmit submit =
        frame.submit({2, 3, 4, 5});

    ASSERT_TRUE(submit.immediate);
    EXPECT_EQ(submit.present_rect.x, 2u);
    EXPECT_EQ(submit.present_rect.y, 3u);
    EXPECT_EQ(submit.present_rect.width, 4u);
    EXPECT_EQ(submit.present_rect.height, 5u);
}

TEST(DisplayFrameTest, AccumulatesPresentRectsInsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({1, 1, 2, 2}).immediate);
    EXPECT_FALSE(frame.submit({0, 20, 80, 18}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    EXPECT_EQ(flush.present_rect.x, 0u);
    EXPECT_EQ(flush.present_rect.y, 1u);
    EXPECT_EQ(flush.present_rect.width, 80u);
    EXPECT_EQ(flush.present_rect.height, 37u);
}

TEST(DisplayFrameTest, ClipsPresentRectsToFrameBounds)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({70, 90, 20, 20}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    EXPECT_EQ(flush.present_rect.x, 70u);
    EXPECT_EQ(flush.present_rect.y, 90u);
    EXPECT_EQ(flush.present_rect.width, 10u);
    EXPECT_EQ(flush.present_rect.height, 10u);
}

TEST(DisplayFrameTest, IgnoresEmptyPresentRects)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    EXPECT_TRUE(flush.present_rect.empty());
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
    EXPECT_FALSE(flush.present_rect.empty());
    EXPECT_EQ(frame.depth(), 0u);
    EXPECT_TRUE(frame.pending().empty());
}
