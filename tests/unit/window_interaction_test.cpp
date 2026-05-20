#include <gtest/gtest.h>

#include "kernel/display/window_interaction.hpp"

namespace
{

void expect_rect(kernel::display::Rect actual,
                 uint64_t x,
                 uint64_t y,
                 uint64_t width,
                 uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

kernel::display::WindowResizeConstraints constraints()
{
    return {
        {0, 0, 800, 600},
        120,
        90,
        kernel::display::terminal_window_frame_config(true),
    };
}

kernel::display::WindowPointerEvent event(kernel::display::WindowChromeHitRegion region,
                                          uint64_t x,
                                          uint64_t y,
                                          bool down)
{
    return {
        {10, 20, 320, 200},
        region,
        x,
        y,
        down,
        constraints(),
    };
}

} // namespace

TEST(WindowInteractionTest, MoveDragComputesMovedBounds)
{
    const kernel::display::WindowMoveDrag drag =
        kernel::display::WindowMoveDrag::begin({10, 20, 320, 200}, 30, 40, {0, 0, 800, 600});

    ASSERT_TRUE(drag.valid());
    expect_rect(drag.bounds_for(80, 90), 60, 70, 320, 200);
}

TEST(WindowInteractionTest, MoveDragClampsToWorkArea)
{
    const kernel::display::WindowMoveDrag drag =
        kernel::display::WindowMoveDrag::begin({10, 20, 320, 200}, 30, 40, {0, 0, 800, 600});

    expect_rect(drag.bounds_for(2000, 2000), 480, 400, 320, 200);
}

TEST(WindowInteractionTest, ResizeDragGrowsBottomRightFromAnchor)
{
    const kernel::display::WindowResizeDrag drag = kernel::display::WindowResizeDrag::begin(
        {10, 20, 320, 200},
        329,
        219,
        kernel::display::WindowResizeEdge::BottomRight,
        constraints());

    ASSERT_TRUE(drag.valid());
    expect_rect(drag.bounds_for(429, 269), 10, 20, 420, 250);
}

TEST(WindowInteractionTest, ResizeDragClampsToWorkArea)
{
    const kernel::display::WindowResizeDrag drag = kernel::display::WindowResizeDrag::begin(
        {10, 20, 320, 200},
        329,
        219,
        kernel::display::WindowResizeEdge::BottomRight,
        constraints());

    expect_rect(drag.bounds_for(2000, 2000), 10, 20, 790, 580);
}

TEST(WindowInteractionTest, ResizeDragSupportsLeftEdge)
{
    const kernel::display::WindowResizeDrag drag = kernel::display::WindowResizeDrag::begin(
        {100, 20, 320, 200},
        100,
        100,
        kernel::display::WindowResizeEdge::Left,
        constraints());

    expect_rect(drag.bounds_for(60, 100), 60, 20, 360, 200);
    expect_rect(drag.bounds_for(380, 100), 298, 20, 122, 200);
}

TEST(WindowInteractionTest, ResizeDragSupportsTopEdge)
{
    const kernel::display::WindowResizeDrag drag = kernel::display::WindowResizeDrag::begin(
        {100, 120, 320, 200},
        200,
        120,
        kernel::display::WindowResizeEdge::Top,
        constraints());

    expect_rect(drag.bounds_for(200, 80), 100, 80, 320, 240);
    expect_rect(drag.bounds_for(200, 280), 100, 209, 320, 111);
}

TEST(WindowInteractionTest, ResizeDragRejectsTooSmallBounds)
{
    const kernel::display::WindowResizeConstraints tight_constraints{
        {0, 0, 100, 80},
        120,
        90,
        kernel::display::terminal_window_frame_config(true),
    };
    const kernel::display::WindowResizeDrag drag = kernel::display::WindowResizeDrag::begin(
        {10, 20, 80, 40},
        89,
        59,
        kernel::display::WindowResizeEdge::BottomRight,
        tight_constraints);

    EXPECT_TRUE(drag.bounds_for(60, 40).empty());
}

TEST(WindowInteractionTest, ResizeDragKeepsMinimumClientCapacity)
{
    const kernel::display::WindowResizeDrag drag = kernel::display::WindowResizeDrag::begin(
        {10, 20, 320, 200},
        329,
        219,
        kernel::display::WindowResizeEdge::BottomRight,
        constraints());

    const kernel::display::Rect bounds = drag.bounds_for(20, 30);
    const kernel::display::WindowFrameMetrics metrics =
        kernel::display::WindowChrome::metrics_for(
            bounds,
            kernel::display::terminal_window_frame_config(true));

    ASSERT_TRUE(metrics.valid());
    EXPECT_GE(metrics.client_bounds.width, 120u);
    EXPECT_GE(metrics.client_bounds.height, 90u);
}

TEST(WindowInteractionTest, CursorShapeMapsChromeRegions)
{
    EXPECT_EQ(kernel::display::cursor_shape_for_hit_region(
                  kernel::display::WindowChromeHitRegion::TitleBar),
              kernel::display::PointerCursorShape::Move);
    EXPECT_EQ(kernel::display::cursor_shape_for_hit_region(
                  kernel::display::WindowChromeHitRegion::ResizeLeft),
              kernel::display::PointerCursorShape::ResizeHorizontal);
    EXPECT_EQ(kernel::display::cursor_shape_for_hit_region(
                  kernel::display::WindowChromeHitRegion::ResizeBottom),
              kernel::display::PointerCursorShape::ResizeVertical);
    EXPECT_EQ(kernel::display::cursor_shape_for_hit_region(
                  kernel::display::WindowChromeHitRegion::ResizeTopRight),
              kernel::display::PointerCursorShape::ResizeDiagonalForward);
    EXPECT_EQ(kernel::display::cursor_shape_for_hit_region(
                  kernel::display::WindowChromeHitRegion::ResizeBottomRight),
              kernel::display::PointerCursorShape::ResizeDiagonalBackward);
    EXPECT_EQ(kernel::display::cursor_shape_for_hit_region(
                  kernel::display::WindowChromeHitRegion::Content),
              kernel::display::PointerCursorShape::Arrow);
}

TEST(WindowInteractionTest, ControllerCommitsMoveOnRelease)
{
    kernel::display::WindowInteractionController controller;

    const kernel::display::WindowInteractionResult press =
        controller.update(event(kernel::display::WindowChromeHitRegion::TitleBar, 30, 40, true));
    EXPECT_TRUE(press.handled);
    EXPECT_TRUE(press.focus_requested);
    EXPECT_EQ(press.mode, kernel::display::WindowInteractionMode::Move);

    const kernel::display::WindowInteractionResult release =
        controller.update(event(kernel::display::WindowChromeHitRegion::TitleBar, 80, 90, false));
    EXPECT_TRUE(release.handled);
    EXPECT_TRUE(release.commit_move);
    expect_rect(release.proposed_bounds, 60, 70, 320, 200);
}

TEST(WindowInteractionTest, ControllerKeepsMoveActiveOutsideTitleBar)
{
    kernel::display::WindowInteractionController controller;

    ASSERT_TRUE(controller.update(event(kernel::display::WindowChromeHitRegion::TitleBar,
                                        30,
                                        40,
                                        true))
                    .handled);

    const kernel::display::WindowInteractionResult drag =
        controller.update(event(kernel::display::WindowChromeHitRegion::None, 90, 110, true));
    EXPECT_TRUE(drag.handled);
    EXPECT_FALSE(drag.commit_move);
    EXPECT_EQ(drag.mode, kernel::display::WindowInteractionMode::Move);
    expect_rect(drag.proposed_bounds, 70, 90, 320, 200);

    const kernel::display::WindowInteractionResult release =
        controller.update(event(kernel::display::WindowChromeHitRegion::None, 90, 110, false));
    EXPECT_TRUE(release.handled);
    EXPECT_TRUE(release.commit_move);
    expect_rect(release.proposed_bounds, 70, 90, 320, 200);
}

TEST(WindowInteractionTest, ControllerRequestsFocusForContentClick)
{
    kernel::display::WindowInteractionController controller;

    const kernel::display::WindowInteractionResult hover =
        controller.update(event(kernel::display::WindowChromeHitRegion::Content, 40, 50, false));
    EXPECT_FALSE(hover.handled);
    EXPECT_FALSE(hover.focus_requested);

    const kernel::display::WindowInteractionResult press =
        controller.update(event(kernel::display::WindowChromeHitRegion::Content, 40, 50, true));
    EXPECT_TRUE(press.handled);
    EXPECT_TRUE(press.focus_requested);
    EXPECT_EQ(press.mode, kernel::display::WindowInteractionMode::None);
}

TEST(WindowInteractionTest, ControllerCommitsResizeOnRelease)
{
    kernel::display::WindowInteractionController controller;

    const kernel::display::WindowInteractionResult press =
        controller.update(event(kernel::display::WindowChromeHitRegion::ResizeBottomRight,
                                329,
                                219,
                                true));
    EXPECT_TRUE(press.handled);
    EXPECT_FALSE(press.focus_requested);
    EXPECT_EQ(press.mode, kernel::display::WindowInteractionMode::Resize);

    const kernel::display::WindowInteractionResult release =
        controller.update(event(kernel::display::WindowChromeHitRegion::ResizeBottomRight,
                                429,
                                269,
                                false));
    EXPECT_TRUE(release.handled);
    EXPECT_TRUE(release.commit_resize);
    expect_rect(release.proposed_bounds, 10, 20, 420, 250);
}

TEST(WindowInteractionTest, ControllerKeepsResizeActiveOutsideResizeHandle)
{
    kernel::display::WindowInteractionController controller;

    ASSERT_TRUE(controller.update(event(kernel::display::WindowChromeHitRegion::ResizeBottomRight,
                                        329,
                                        219,
                                        true))
                    .handled);

    const kernel::display::WindowInteractionResult drag =
        controller.update(event(kernel::display::WindowChromeHitRegion::None, 460, 290, true));
    EXPECT_TRUE(drag.handled);
    EXPECT_FALSE(drag.commit_resize);
    EXPECT_EQ(drag.mode, kernel::display::WindowInteractionMode::Resize);
    expect_rect(drag.proposed_bounds, 10, 20, 451, 271);

    const kernel::display::WindowInteractionResult release =
        controller.update(event(kernel::display::WindowChromeHitRegion::None, 460, 290, false));
    EXPECT_TRUE(release.handled);
    EXPECT_TRUE(release.commit_resize);
    expect_rect(release.proposed_bounds, 10, 20, 451, 271);
}

TEST(WindowInteractionTest, ControllerCloseRequiresReleaseOverCloseButton)
{
    kernel::display::WindowInteractionController controller;

    const kernel::display::WindowInteractionResult press =
        controller.update(event(kernel::display::WindowChromeHitRegion::CloseButton, 318, 30, true));
    EXPECT_TRUE(press.handled);
    EXPECT_FALSE(press.focus_requested);
    EXPECT_TRUE(controller
                    .update(event(kernel::display::WindowChromeHitRegion::CloseButton, 318, 30, false))
                    .close_requested);
}

TEST(WindowInteractionTest, ReleaseWithoutActiveDragIsNoOp)
{
    kernel::display::WindowInteractionController controller;

    const kernel::display::WindowInteractionResult result =
        controller.update(event(kernel::display::WindowChromeHitRegion::Content, 40, 50, false));

    EXPECT_FALSE(result.handled);
    EXPECT_FALSE(result.commit_move);
    EXPECT_FALSE(result.commit_resize);
    EXPECT_FALSE(result.close_requested);
}

TEST(WindowInteractionTest, ResetClearsActiveMode)
{
    kernel::display::WindowInteractionController controller;
    ASSERT_TRUE(controller.update(event(kernel::display::WindowChromeHitRegion::TitleBar,
                                        30,
                                        40,
                                        true))
                    .handled);
    ASSERT_TRUE(controller.active());

    controller.reset();

    EXPECT_FALSE(controller.active());
    const kernel::display::WindowInteractionResult result =
        controller.update(event(kernel::display::WindowChromeHitRegion::Content, 50, 50, false));
    EXPECT_FALSE(result.handled);
}

TEST(WindowInteractionTest, ReplayProfilesMoveAsPreviewThenSingleCommit)
{
    const kernel::display::WindowInteractionReplay replay(
        {0, 0, 800, 600},
        kernel::display::terminal_window_frame_config(true));

    const kernel::display::WindowInteractionReplayStats stats =
        replay.profile_preview_then_commit({
            kernel::display::WindowInteractionReplayKind::Move,
            {10, 20, 320, 200},
            kernel::display::WindowChromeHitRegion::TitleBar,
            30,
            40,
            12,
            6,
            10,
            constraints(),
        });

    EXPECT_EQ(stats.pointer_event_count, 12u);
    EXPECT_EQ(stats.preview_update_count, 10u);
    EXPECT_EQ(stats.commit_count, 1u);
    EXPECT_GT(stats.preview_repaint_pixels, 0u);
    EXPECT_GT(stats.commit_repaint_pixels, 0u);
    EXPECT_EQ(stats.full_screen_fallback_count, 0u);
    EXPECT_LT(stats.largest_repaint_area, 800u * 600u);
}

TEST(WindowInteractionTest, LiveMoveCoalescerCommitsFirstStrideAndFinalBounds)
{
    kernel::display::WindowLiveMoveCoalescer coalescer(3);

    kernel::display::WindowCoalescedMoveResult result =
        coalescer.update({10, 20, 100, 80}, false);
    EXPECT_TRUE(result.commit);
    EXPECT_EQ(result.bounds.x, 10u);

    result = coalescer.update({20, 20, 100, 80}, false);
    EXPECT_FALSE(result.commit);

    result = coalescer.update({30, 20, 100, 80}, false);
    EXPECT_FALSE(result.commit);

    result = coalescer.update({40, 20, 100, 80}, false);
    EXPECT_TRUE(result.commit);
    EXPECT_EQ(result.bounds.x, 40u);

    result = coalescer.update({50, 20, 100, 80}, true);
    EXPECT_TRUE(result.commit);
    EXPECT_EQ(result.bounds.x, 50u);

    result = coalescer.update({50, 20, 100, 80}, true);
    EXPECT_FALSE(result.commit);
}

TEST(WindowInteractionTest, ReplayProfilesLiveMoveCoalescedWithoutPreview)
{
    const kernel::display::WindowInteractionReplay replay(
        {0, 0, 800, 600},
        kernel::display::terminal_window_frame_config(true));

    const kernel::display::WindowInteractionReplayStats stats = replay.profile_live_move({
        kernel::display::WindowInteractionReplayKind::Move,
        {10, 20, 320, 200},
        kernel::display::WindowChromeHitRegion::TitleBar,
        30,
        40,
        12,
        6,
        10,
        constraints(),
    });

    EXPECT_EQ(stats.pointer_event_count, 12u);
    EXPECT_EQ(stats.preview_update_count, 0u);
    EXPECT_EQ(stats.commit_count, 4u);
    EXPECT_EQ(stats.preview_repaint_pixels, 0u);
    EXPECT_GT(stats.commit_repaint_pixels, 0u);
    EXPECT_EQ(stats.full_screen_fallback_count, 0u);
    EXPECT_LT(stats.largest_repaint_area, 800u * 600u);
}

TEST(WindowInteractionTest, ReplayProfilesResizeAsPreviewThenSingleCommit)
{
    const kernel::display::WindowInteractionReplay replay(
        {0, 0, 800, 600},
        kernel::display::terminal_window_frame_config(true));

    const kernel::display::WindowInteractionReplayStats stats =
        replay.profile_preview_then_commit({
            kernel::display::WindowInteractionReplayKind::Resize,
            {10, 20, 320, 200},
            kernel::display::WindowChromeHitRegion::ResizeBottomRight,
            329,
            219,
            9,
            5,
            8,
            constraints(),
        });

    EXPECT_EQ(stats.pointer_event_count, 10u);
    EXPECT_EQ(stats.preview_update_count, 8u);
    EXPECT_EQ(stats.commit_count, 1u);
    EXPECT_GT(stats.preview_repaint_pixels, 0u);
    EXPECT_GT(stats.commit_repaint_pixels, 0u);
    EXPECT_EQ(stats.full_screen_fallback_count, 0u);
    EXPECT_LT(stats.largest_repaint_area, 800u * 600u);
}
