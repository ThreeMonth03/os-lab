#include <gtest/gtest.h>

#include "kernel/display/composited_surface.hpp"
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
    const kernel::display::LayerOcclusion occlusion =
        kind == kernel::display::LayerKind::MouseCursor ? kernel::display::LayerOcclusion::Transparent
                                                        : kernel::display::LayerOcclusion::Opaque;
    return {kind, id, {0, 0, 640, 480}, true, occlusion};
}

kernel::display::Layer bounded_layer(kernel::display::LayerKind kind,
                                     kernel::display::SurfaceId id,
                                     kernel::display::Rect bounds,
                                     bool visible = true,
                                     kernel::display::LayerOcclusion occlusion = kernel::display::LayerOcclusion::Opaque)
{
    return {kind, id, bounds, visible, occlusion};
}

kernel::display::Layer cursor_layer(kernel::display::Rect bounds = {0, 0, 800, 600})
{
    return bounded_layer(kernel::display::LayerKind::MouseCursor,
                         kernel::display::kMouseCursorLayerSurfaceId,
                         bounds,
                         true,
                         kernel::display::LayerOcclusion::Transparent);
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

TEST(DisplayCompositorTest, OrdersDesktopGuiAppOverlayAndMouseCursorLayers)
{
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::GuiSurface,
                                             kernel::display::LayerKind::DesktopBackground));
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::AppSurface,
                                             kernel::display::LayerKind::GuiSurface));
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::DebugOverlay,
                                             kernel::display::LayerKind::AppSurface));
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::MouseCursor,
                                             kernel::display::LayerKind::DebugOverlay));
    EXPECT_FALSE(kernel::display::layer_above(kernel::display::LayerKind::DesktopBackground,
                                              kernel::display::LayerKind::MouseCursor));
}

TEST(DisplayCompositorTest, RegistersDesktopGuiAppAndOverlayLayers)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::DesktopBackground, 100)));
    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::GuiSurface, 150)));
    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::AppSurface, 200)));
    EXPECT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::DebugOverlay,
                                                kernel::display::debug_overlay::kSurfaceId)));
    EXPECT_FALSE(compositor.register_layer(layer(kernel::display::LayerKind::DebugOverlay, 42)));

    EXPECT_EQ(compositor.layer_count(), 4u);
    ASSERT_NE(compositor.find_layer(kernel::display::LayerKind::DesktopBackground), nullptr);
    ASSERT_NE(compositor.find_layer(kernel::display::LayerKind::GuiSurface), nullptr);
    ASSERT_NE(compositor.find_layer(kernel::display::LayerKind::AppSurface), nullptr);
    ASSERT_NE(compositor.find_layer(kernel::display::LayerKind::DebugOverlay), nullptr);

    const kernel::display::Layer * top = compositor.top_visible_layer();
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->kind, kernel::display::LayerKind::DebugOverlay);
}

TEST(DisplayCompositorTest, RegistersCompositedSurfaceDescriptors)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_surface(kernel::display::make_composited_surface(
        100,
        kernel::display::CompositedSurfaceRole::Background,
        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_surface(kernel::display::make_composited_surface(
        150,
        kernel::display::CompositedSurfaceRole::SystemUi,
        {10, 10, 120, 40})));
    ASSERT_TRUE(compositor.register_surface(kernel::display::make_composited_surface(
        200,
        kernel::display::CompositedSurfaceRole::App,
        {0, 0, 800, 600},
        true,
        true,
        true)));
    ASSERT_TRUE(compositor.register_surface(kernel::display::make_composited_surface(
        kernel::display::debug_overlay::kSurfaceId,
        kernel::display::CompositedSurfaceRole::Overlay,
        {700, 0, 80, 20})));
    ASSERT_TRUE(compositor.register_surface(kernel::display::make_composited_surface(
        kernel::display::kMouseCursorLayerSurfaceId,
        kernel::display::CompositedSurfaceRole::Cursor,
        {0, 0, 800, 600})));

    EXPECT_EQ(compositor.layer_count(), 5u);
    const kernel::display::Layer * cursor =
        compositor.find_layer(kernel::display::LayerKind::MouseCursor);
    ASSERT_NE(cursor, nullptr);
    EXPECT_FALSE(cursor->occludes_lower_repaint());

    const kernel::display::Layer * top = compositor.top_visible_layer();
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->kind, kernel::display::LayerKind::MouseCursor);
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

TEST(DisplayCompositorTest, MarksOccludingLayerMetadata)
{
    const kernel::display::Layer overlay = bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                         kernel::display::debug_overlay::kSurfaceId,
                                                         {700, 0, 80, 20});
    const kernel::display::Layer app = bounded_layer(kernel::display::LayerKind::AppSurface,
                                                     200,
                                                     {0, 0, 800, 600});
    const kernel::display::Layer cursor = cursor_layer();

    EXPECT_TRUE(overlay.occludes_lower_repaint());
    EXPECT_TRUE(app.occludes_lower_repaint());
    EXPECT_FALSE(cursor.occludes_lower_repaint());
}

TEST(DisplayCompositorTest, TransparentCursorDoesNotOccludeAppRepaint)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopBackground,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(cursor_layer({100, 100, 16, 16})));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_from(kernel::display::LayerKind::DesktopBackground, {104, 104, 4, 4});

    ASSERT_EQ(plan.count, 2u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::AppSurface);
    expect_rect(plan.rect_at(0), 104, 104, 4, 4);
    EXPECT_EQ(plan.at(1), kernel::display::LayerKind::MouseCursor);
    expect_rect(plan.rect_at(1), 104, 104, 4, 4);
}

TEST(DisplayCompositorTest, SkipsAppLayerWhenDirtyRectIsCoveredByOpaqueOverlay)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                        kernel::display::debug_overlay::kSurfaceId,
                                                        {700, 0, 80, 20})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopBackground,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(cursor_layer()));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_from(kernel::display::LayerKind::DesktopBackground, {704, 4, 10, 10});

    ASSERT_EQ(plan.count, 2u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::DebugOverlay);
    expect_rect(plan.rect_at(0), 704, 4, 10, 10);
    EXPECT_EQ(plan.at(1), kernel::display::LayerKind::MouseCursor);
    expect_rect(plan.rect_at(1), 704, 4, 10, 10);
    EXPECT_FALSE(plan.contains(kernel::display::LayerKind::AppSurface));
}

TEST(DisplayCompositorTest, CursorDirtyOverPanelPlansDesktopGuiAndCursor)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopBackground,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::GuiSurface,
                                                        150,
                                                        {10, 10, 80, 40})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {10, 60, 780, 500})));
    ASSERT_TRUE(compositor.register_layer(cursor_layer()));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_from(kernel::display::LayerKind::DesktopBackground, {12, 12, 10, 16});

    ASSERT_EQ(plan.count, 2u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::GuiSurface);
    EXPECT_EQ(plan.at(1), kernel::display::LayerKind::MouseCursor);
}

TEST(DisplayCompositorTest, RepaintsAppLayerOutsideOpaqueOverlay)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopBackground,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                        kernel::display::debug_overlay::kSurfaceId,
                                                        {700, 0, 80, 20})));
    ASSERT_TRUE(compositor.register_layer(cursor_layer()));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_from(kernel::display::LayerKind::DesktopBackground, {12, 12, 4, 4});

    ASSERT_EQ(plan.count, 2u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::AppSurface);
    expect_rect(plan.rect_at(0), 12, 12, 4, 4);
    EXPECT_EQ(plan.at(1), kernel::display::LayerKind::MouseCursor);
}

TEST(DisplayCompositorTest, SplitsAppRepaintAroundOpaqueOverlay)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});
    const kernel::display::Rect overlay_bounds{700, 0, 80, 20};

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopBackground,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                        kernel::display::debug_overlay::kSurfaceId,
                                                        overlay_bounds)));
    ASSERT_TRUE(compositor.register_layer(cursor_layer()));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_from(kernel::display::LayerKind::DesktopBackground, {690, 10, 40, 20});

    ASSERT_EQ(plan.count, 4u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::AppSurface);
    EXPECT_FALSE(kernel::display::rects_overlap(plan.rect_at(0), overlay_bounds));
    EXPECT_EQ(plan.at(1), kernel::display::LayerKind::AppSurface);
    EXPECT_FALSE(kernel::display::rects_overlap(plan.rect_at(1), overlay_bounds));
    EXPECT_EQ(plan.at(2), kernel::display::LayerKind::DebugOverlay);
    expect_rect(plan.rect_at(2), 700, 10, 30, 10);
    EXPECT_EQ(plan.at(3), kernel::display::LayerKind::MouseCursor);
}

TEST(DisplayCompositorTest, PlansOnlyHigherIntersectingLayersAfterAppUpdate)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopBackground,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                        kernel::display::debug_overlay::kSurfaceId,
                                                        {700, 0, 80, 20})));
    ASSERT_TRUE(compositor.register_layer(cursor_layer()));

    const kernel::display::LayerRepaintPlan plan =
        compositor.repaint_plan_above(kernel::display::LayerKind::AppSurface, {12, 12, 4, 4});

    ASSERT_EQ(plan.count, 1u);
    EXPECT_EQ(plan.at(0), kernel::display::LayerKind::MouseCursor);
}

TEST(DisplayCompositorTest, FullAppRepaintRestoresOverlayAndCursorLayers)
{
    kernel::display::Compositor compositor({0, 0, 800, 600});

    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DesktopBackground,
                                                        100,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::AppSurface,
                                                        200,
                                                        {0, 0, 800, 600})));
    ASSERT_TRUE(compositor.register_layer(bounded_layer(kernel::display::LayerKind::DebugOverlay,
                                                        kernel::display::debug_overlay::kSurfaceId,
                                                        {700, 0, 80, 20})));
    ASSERT_TRUE(compositor.register_layer(cursor_layer()));

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
