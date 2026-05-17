#include <gtest/gtest.h>

#include "kernel/display/display_stats.hpp"

TEST(DisplayStatsTest, ComputesPipelineDelta)
{
    kernel::display::DisplayPipelineStats before;
    before.frame.frame_flush_count = 2;
    before.frame.present_rect_count = 3;
    before.presenter.fast_path_copy_pixels = 100;
    before.compositor.scene_compose_pixels = 20;
    before.compositor.scene_preflight_pixels = 7;

    kernel::display::DisplayPipelineStats after;
    after.frame.frame_flush_count = 5;
    after.frame.present_rect_count = 9;
    after.presenter.fast_path_copy_pixels = 180;
    after.compositor.scene_compose_pixels = 35;
    after.compositor.scene_preflight_pixels = 7;

    const kernel::display::DisplayPipelineStats delta =
        kernel::display::display_stats_delta(
            kernel::display::make_display_stats_snapshot(before),
            kernel::display::make_display_stats_snapshot(after));

    EXPECT_EQ(delta.frame.frame_flush_count, 3u);
    EXPECT_EQ(delta.frame.present_rect_count, 6u);
    EXPECT_EQ(delta.presenter.fast_path_copy_pixels, 80u);
    EXPECT_EQ(delta.compositor.scene_compose_pixels, 15u);
    EXPECT_EQ(delta.compositor.scene_preflight_pixels, 0u);
}

TEST(DisplayStatsTest, DeltaSaturatesWhenCountersReset)
{
    kernel::display::DisplayPipelineStats before;
    before.frame.total_presented_pixels = 100;
    before.presenter.overlay_blend_pixels = 50;
    before.compositor.repaint_plan_count = 4;

    kernel::display::DisplayPipelineStats after;
    after.frame.total_presented_pixels = 10;
    after.presenter.overlay_blend_pixels = 5;
    after.compositor.repaint_plan_count = 1;

    const kernel::display::DisplayPipelineStats delta =
        kernel::display::display_stats_delta(
            kernel::display::make_display_stats_snapshot(before),
            kernel::display::make_display_stats_snapshot(after));

    EXPECT_EQ(delta.frame.total_presented_pixels, 0u);
    EXPECT_EQ(delta.presenter.overlay_blend_pixels, 0u);
    EXPECT_EQ(delta.compositor.repaint_plan_count, 0u);
}
