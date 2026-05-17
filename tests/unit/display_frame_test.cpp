#include <gtest/gtest.h>

#include "kernel/display/display_frame.hpp"

TEST(DisplayFrameTest, SubmitsImmediatelyOutsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});

    const kernel::display::DisplayFrameSubmit submit =
        frame.submit({{2, 3, 4, 5}, {}});

    ASSERT_TRUE(submit.immediate);
    EXPECT_EQ(submit.damage.dirty_rect.x, 2u);
    EXPECT_EQ(submit.damage.dirty_rect.y, 3u);
    EXPECT_EQ(submit.damage.dirty_rect.width, 4u);
    EXPECT_EQ(submit.damage.dirty_rect.height, 5u);
    EXPECT_FALSE(submit.damage.has_scroll());
}

TEST(DisplayFrameTest, AccumulatesDirtyAndScrollInsideFrame)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({{1, 1, 2, 2}, {}}).immediate);
    EXPECT_FALSE(frame.submit({{}, {{0, 0, 80, 90}, 18}}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    EXPECT_TRUE(flush.damage.has_dirty());
    EXPECT_TRUE(flush.damage.has_scroll());
    EXPECT_EQ(flush.damage.scroll.distance, 18u);
    EXPECT_EQ(flush.damage.scroll.rect.height, 90u);
}

TEST(DisplayFrameTest, KeepsDirtyBeforeScrollOrdering)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({{0, 72, 80, 18}, {}}).immediate);
    EXPECT_FALSE(frame.submit({{}, {{0, 0, 80, 90}, 18}}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_EQ(flush.damage.step_count, 3u);
    EXPECT_EQ(flush.damage.steps[0].kind, kernel::display::FrameDamageStepKind::DirtyRect);
    EXPECT_EQ(flush.damage.steps[1].kind, kernel::display::FrameDamageStepKind::Scroll);
    EXPECT_EQ(flush.damage.steps[2].kind, kernel::display::FrameDamageStepKind::DirtyRect);
}

TEST(DisplayFrameTest, CollapsesMultipleScrolls)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();

    EXPECT_FALSE(frame.submit({{}, {{0, 0, 80, 90}, 18}}).immediate);
    EXPECT_FALSE(frame.submit({{}, {{0, 0, 80, 90}, 18}}).immediate);
    EXPECT_FALSE(frame.submit({{}, {{0, 0, 80, 90}, 18}}).immediate);

    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    ASSERT_TRUE(flush.damage.has_scroll());
    EXPECT_EQ(flush.damage.scroll.distance, 54u);
}

TEST(DisplayFrameTest, NestedFrameFlushesOnlyAtOutermostEnd)
{
    kernel::display::DisplayFrame frame({0, 0, 80, 100});
    frame.begin();
    frame.begin();

    EXPECT_FALSE(frame.submit({{4, 4, 8, 8}, {}}).immediate);

    EXPECT_FALSE(frame.end().outermost_frame_ended);
    const kernel::display::DisplayFrameFlush flush = frame.end();

    ASSERT_TRUE(flush.outermost_frame_ended);
    EXPECT_TRUE(flush.damage.has_dirty());
    EXPECT_EQ(frame.depth(), 0u);
    EXPECT_TRUE(frame.pending().empty());
}
