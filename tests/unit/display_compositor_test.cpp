#include <gtest/gtest.h>

#include "kernel/display/compositor.hpp"
#include "kernel/display/debug_overlay.hpp"

namespace
{

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

kernel::display::Layer layer(kernel::display::LayerKind kind, kernel::display::SurfaceId id)
{
    return {kind, id, {0, 0, 640, 480}, true};
}

kernel::display::Layer bounded_layer(kernel::display::LayerKind kind,
                                     kernel::display::SurfaceId id,
                                     kernel::display::Rect bounds,
                                     bool visible = true)
{
    return {kind, id, bounds, visible};
}

} // namespace

TEST(DisplayCompositorTest, ClipsDirtyRectsToFramebufferBounds)
{
    kernel::display::DirtyRectQueue queue({0, 0, 100, 50});

    EXPECT_EQ(queue.mark_dirty({90, 40, 20, 20}), kernel::display::DirtyMarkResult::Queued);

    kernel::display::Rect dirty;
    ASSERT_TRUE(queue.pop(dirty));
    expect_rect(dirty, 90, 40, 10, 10);
    EXPECT_FALSE(queue.pop(dirty));
}

TEST(DisplayCompositorTest, MergesTouchingDirtyRects)
{
    kernel::display::DirtyRectQueue queue({0, 0, 100, 50});

    EXPECT_EQ(queue.mark_dirty({10, 10, 10, 10}), kernel::display::DirtyMarkResult::Queued);
    EXPECT_EQ(queue.mark_dirty({20, 10, 5, 10}), kernel::display::DirtyMarkResult::Merged);
    EXPECT_EQ(queue.size(), 1u);

    kernel::display::Rect dirty;
    ASSERT_TRUE(queue.pop(dirty));
    expect_rect(dirty, 10, 10, 15, 10);
}

TEST(DisplayCompositorTest, FallsBackToFullscreenDirtyWhenQueueIsFull)
{
    kernel::display::DirtyRectQueue queue({0, 0, 100, 50});

    for (size_t index = 0; index < queue.capacity(); ++index)
    {
        EXPECT_EQ(queue.mark_dirty({index * 2, 0, 1, 1}), kernel::display::DirtyMarkResult::Queued);
    }

    EXPECT_EQ(queue.mark_dirty({90, 40, 1, 1}), kernel::display::DirtyMarkResult::FullscreenFallback);
    EXPECT_TRUE(queue.full_screen_dirty());
    EXPECT_EQ(queue.size(), 1u);

    kernel::display::Rect dirty;
    ASSERT_TRUE(queue.pop(dirty));
    expect_rect(dirty, 0, 0, 100, 50);
}

TEST(DisplayCompositorTest, CursorMoveMarksOldAndNewRectsDirty)
{
    kernel::display::DirtyRectQueue queue({0, 0, 100, 50});

    kernel::display::mark_cursor_move_dirty(queue, {4, 4, 10, 16}, {40, 4, 10, 16});

    EXPECT_EQ(queue.size(), 2u);
    kernel::display::Rect dirty;
    ASSERT_TRUE(queue.pop(dirty));
    expect_rect(dirty, 4, 4, 10, 16);
    ASSERT_TRUE(queue.pop(dirty));
    expect_rect(dirty, 40, 4, 10, 16);
    EXPECT_FALSE(queue.pop(dirty));
}

TEST(DisplayCompositorTest, OrdersDesktopAppOverlayAndMouseCursorLayers)
{
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::AppSurface,
                                             kernel::display::LayerKind::DesktopPanel));
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::DebugOverlay,
                                             kernel::display::LayerKind::AppSurface));
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::MouseCursor,
                                             kernel::display::LayerKind::DebugOverlay));
    EXPECT_FALSE(kernel::display::layer_above(kernel::display::LayerKind::DesktopPanel,
                                              kernel::display::LayerKind::MouseCursor));
}

TEST(DisplayCompositorTest, RegistersDesktopAppAndOverlayLayers)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::DesktopPanel, 100)));
    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::AppSurface, 200)));
    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::DebugOverlay,
                                                kernel::display::debug_overlay::kSurfaceId)));
    EXPECT_FALSE(compositor.register_layer(layer(kernel::display::LayerKind::DebugOverlay, 42)));

    EXPECT_EQ(compositor.layer_count(), 3u);
    ASSERT_NE(compositor.find_layer(kernel::display::LayerKind::DesktopPanel), nullptr);
    ASSERT_NE(compositor.find_layer(kernel::display::LayerKind::AppSurface), nullptr);
    ASSERT_NE(compositor.find_layer(kernel::display::LayerKind::DebugOverlay), nullptr);

    const kernel::display::Layer * top = compositor.top_visible_layer();
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->kind, kernel::display::LayerKind::DebugOverlay);
}

TEST(DisplayCompositorTest, MouseCursorLayerIsTopmost)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::DebugOverlay,
                                                kernel::display::debug_overlay::kSurfaceId)));
    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::MouseCursor,
                                                kernel::display::kMouseCursorLayerSurfaceId)));
    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::AppSurface, 200)));

    const kernel::display::Layer * top = compositor.top_visible_layer();
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->kind, kernel::display::LayerKind::MouseCursor);
}

TEST(DisplayCompositorTest, PlansRepaintOrderFromDesktopToCursor)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::MouseCursor,
                                                        kernel::display::kMouseCursorLayerSurfaceId,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                        kernel::display::debug_overlay::kSurfaceId,
                                                        {0, 0, 120, 20})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopPanel,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {10, 10, 780, 580})));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_from(kernel::display::LayerKind::DesktopPanel, {12, 12, 4, 4});

    ASSERT_EQ(plan.count, 4u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::DesktopPanel);
    EXPECT_EQ(plan.at(1), kernel::display::LayerKind::AppSurface);
    EXPECT_EQ(plan.at(2), kernel::display::LayerKind::DebugOverlay);
    EXPECT_EQ(plan.at(3), kernel::display::LayerKind::MouseCursor);
}

TEST(DisplayCompositorTest, CursorDirtyOverPanelPlansDesktopBeforeAppAndCursor)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopPanel,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {10, 10, 80, 40})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::MouseCursor,
                                                        kernel::display::kMouseCursorLayerSurfaceId,
                                                        {0, 0, 800, 600})));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_from(kernel::display::LayerKind::DesktopPanel, {12, 12, 10, 16});

    ASSERT_EQ(plan.count, 3u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::DesktopPanel);
    EXPECT_EQ(plan.at(1), kernel::display::LayerKind::AppSurface);
    EXPECT_EQ(plan.at(2), kernel::display::LayerKind::MouseCursor);
}

TEST(DisplayCompositorTest, PlansOnlyHigherIntersectingLayersAfterAppUpdate)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopPanel,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                        kernel::display::debug_overlay::kSurfaceId,
                                                        {700, 0, 80, 20})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::MouseCursor,
                                                        kernel::display::kMouseCursorLayerSurfaceId,
                                                        {0, 0, 800, 600})));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_above(kernel::display::LayerKind::AppSurface, {12, 12, 4, 4});

    ASSERT_EQ(plan.count, 1u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::MouseCursor);
}

TEST(DisplayCompositorTest, FullAppRepaintRestoresOverlayAndCursorLayers)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopPanel,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                        kernel::display::debug_overlay::kSurfaceId,
                                                        {700, 0, 80, 20})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::MouseCursor,
                                                        kernel::display::kMouseCursorLayerSurfaceId,
                                                        {0, 0, 800, 600})));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_above(kernel::display::LayerKind::AppSurface, {0, 0, 800, 600});

    ASSERT_EQ(plan.count, 2u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::DebugOverlay);
    EXPECT_EQ(plan.at(1), kernel::display::LayerKind::MouseCursor);
}

TEST(DisplayCompositorTest, RepaintsOnlyVisibleHigherLayersIntersectingDirtyRect)
{
    const kernel::display::Layer overlay = bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                         100,
                                                         {10, 10, 50, 50});
    const kernel::display::Layer hidden_overlay = bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                                2,
                                                                {0, 0, 80, 20},
                                                                false);
    const kernel::display::Layer app = bounded_layer(kernel::display::LayerKind::AppSurface,
                                                     200,
                                                     {0, 0, 800, 600});

    EXPECT_TRUE(kernel::display::should_repaint_layer_after_update(overlay,
                                                                   kernel::display::LayerKind::AppSurface,
                                                                   {20, 20, 4, 4}));
    EXPECT_FALSE(kernel::display::should_repaint_layer_after_update(overlay,
                                                                    kernel::display::LayerKind::AppSurface,
                                                                    {80, 80, 4, 4}));
    EXPECT_FALSE(kernel::display::should_repaint_layer_after_update(hidden_overlay,
                                                                    kernel::display::LayerKind::AppSurface,
                                                                    {1, 1, 4, 4}));
    EXPECT_FALSE(kernel::display::should_repaint_layer_after_update(app,
                                                                    kernel::display::LayerKind::DebugOverlay,
                                                                    {1, 1, 4, 4}));
}
