#include <gtest/gtest.h>

#include "kernel/display/display_stats.hpp"

TEST(DisplayStatsTest, ComputesPipelineDelta)
{
    kernel::display::DisplayPipelineStats before;
    before.frame.frame_flush_count = 2;
    before.frame.present_operation_count = 4;
    before.frame.present_rect_count = 3;
    before.frame.present_scroll_count = 1;
    before.presenter.fast_path_copy_pixels = 100;
    before.presenter.front_scroll_copy_pixels = 40;
    before.compositor.scene_compose_pixels = 20;
    before.compositor.scene_compose_from_backing_pixels = 11;
    before.compositor.scene_preflight_pixels = 7;
    before.runtime.terminal_backing_copy_pixels = 30;
    before.runtime.window_repaint_request_count = 2;
    before.runtime.window_repaint_pixels = 1000;
    before.runtime.window_largest_repaint_area = 700;
    before.runtime.window_move_repaint_pixels = 800;
    before.runtime.window_visual_repaint_pixels = 120;
    before.runtime.window_preview_repaint_pixels = 40;
    before.runtime.window_interaction_pointer_events = 3;
    before.elapsed_ticks = 2;

    kernel::display::DisplayPipelineStats after;
    after.frame.frame_flush_count = 5;
    after.frame.present_operation_count = 11;
    after.frame.present_rect_count = 9;
    after.frame.present_scroll_count = 3;
    after.presenter.fast_path_copy_pixels = 180;
    after.presenter.front_scroll_copy_pixels = 100;
    after.compositor.scene_compose_pixels = 35;
    after.compositor.scene_compose_from_backing_pixels = 25;
    after.compositor.scene_preflight_pixels = 7;
    after.runtime.terminal_backing_copy_pixels = 90;
    after.runtime.window_repaint_request_count = 5;
    after.runtime.window_repaint_pixels = 1500;
    after.runtime.window_largest_repaint_area = 900;
    after.runtime.window_move_repaint_pixels = 1200;
    after.runtime.window_visual_repaint_pixels = 240;
    after.runtime.window_preview_repaint_pixels = 120;
    after.runtime.window_interaction_pointer_events = 13;
    after.elapsed_ticks = 9;

    const kernel::display::DisplayPipelineStats delta =
        kernel::display::display_stats_delta(
            kernel::display::make_display_stats_snapshot(before),
            kernel::display::make_display_stats_snapshot(after));

    EXPECT_EQ(delta.frame.frame_flush_count, 3u);
    EXPECT_EQ(delta.frame.present_operation_count, 7u);
    EXPECT_EQ(delta.frame.present_rect_count, 6u);
    EXPECT_EQ(delta.frame.present_scroll_count, 2u);
    EXPECT_EQ(delta.presenter.fast_path_copy_pixels, 80u);
    EXPECT_EQ(delta.presenter.front_scroll_copy_pixels, 60u);
    EXPECT_EQ(delta.compositor.scene_compose_pixels, 15u);
    EXPECT_EQ(delta.compositor.scene_compose_from_backing_pixels, 14u);
    EXPECT_EQ(delta.compositor.scene_preflight_pixels, 0u);
    EXPECT_EQ(delta.runtime.terminal_backing_copy_pixels, 60u);
    EXPECT_EQ(delta.runtime.window_repaint_request_count, 3u);
    EXPECT_EQ(delta.runtime.window_repaint_pixels, 500u);
    EXPECT_EQ(delta.runtime.window_largest_repaint_area, 200u);
    EXPECT_EQ(delta.runtime.window_move_repaint_pixels, 400u);
    EXPECT_EQ(delta.runtime.window_visual_repaint_pixels, 120u);
    EXPECT_EQ(delta.runtime.window_preview_repaint_pixels, 80u);
    EXPECT_EQ(delta.runtime.window_interaction_pointer_events, 10u);
    EXPECT_EQ(delta.elapsed_ticks, 7u);
}

TEST(DisplayStatsTest, DeltaSaturatesWhenCountersReset)
{
    kernel::display::DisplayPipelineStats before;
    before.frame.total_presented_pixels = 100;
    before.presenter.overlay_blend_pixels = 50;
    before.compositor.repaint_plan_count = 4;
    before.runtime.terminal_backing_copy_pixels = 12;
    before.runtime.window_repaint_pixels = 20;

    kernel::display::DisplayPipelineStats after;
    after.frame.total_presented_pixels = 10;
    after.presenter.overlay_blend_pixels = 5;
    after.compositor.repaint_plan_count = 1;
    after.runtime.terminal_backing_copy_pixels = 3;
    after.runtime.window_repaint_pixels = 2;

    const kernel::display::DisplayPipelineStats delta =
        kernel::display::display_stats_delta(
            kernel::display::make_display_stats_snapshot(before),
            kernel::display::make_display_stats_snapshot(after));

    EXPECT_EQ(delta.frame.total_presented_pixels, 0u);
    EXPECT_EQ(delta.presenter.overlay_blend_pixels, 0u);
    EXPECT_EQ(delta.compositor.repaint_plan_count, 0u);
    EXPECT_EQ(delta.runtime.terminal_backing_copy_pixels, 0u);
    EXPECT_EQ(delta.runtime.window_repaint_pixels, 0u);
}
