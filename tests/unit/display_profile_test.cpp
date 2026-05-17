#include <gtest/gtest.h>

#include "kernel/display/display_profile.hpp"
#include "test_helpers.hpp"

TEST(DisplayProfileTest, CommandProfileDeltaIsReportedAfterFinish)
{
    kernel::display::CommandProfileTracker tracker;

    kernel::display::DisplayPipelineStats before;
    before.frame.frame_flush_count = 1;
    before.frame.present_rect_count = 2;
    before.presenter.fast_path_copy_pixels = 10;
    before.compositor.scene_compose_pixels = 20;

    kernel::display::DisplayPipelineStats after = before;
    after.frame.frame_flush_count = 2;
    after.frame.present_rect_count = 5;
    after.presenter.fast_path_copy_pixels = 40;
    after.compositor.scene_compose_pixels = 35;

    tracker.begin("input", before);
    const kernel::display::CommandProfileDelta delta = tracker.finish(after);

    ASSERT_TRUE(delta.valid);
    os_lab::test::expect_text(delta.command, "input");
    EXPECT_EQ(delta.delta.frame.frame_flush_count, 1u);
    EXPECT_EQ(delta.delta.frame.present_rect_count, 3u);
    EXPECT_EQ(delta.delta.presenter.fast_path_copy_pixels, 30u);
    EXPECT_EQ(delta.delta.compositor.scene_compose_pixels, 15u);
    EXPECT_FALSE(tracker.pending());
}

TEST(DisplayProfileTest, FinishWithoutPendingProfileReturnsInvalidDelta)
{
    kernel::display::CommandProfileTracker tracker;
    const kernel::display::CommandProfileDelta delta = tracker.finish({});

    EXPECT_FALSE(delta.valid);
}

TEST(DisplayProfileTest, CommandNameIsCopiedBeforeOriginalBufferChanges)
{
    kernel::display::CommandProfileTracker tracker;
    char command[] = {'h', 'e', 'l', 'p'};

    tracker.begin({command, sizeof(command)}, {});
    command[0] = 'x';
    const kernel::display::CommandProfileDelta delta = tracker.finish({});

    ASSERT_TRUE(delta.valid);
    os_lab::test::expect_text(delta.command, "help");
}
