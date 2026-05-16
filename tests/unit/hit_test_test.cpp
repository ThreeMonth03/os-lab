#include <gtest/gtest.h>

#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/hit_test.hpp"

namespace
{

kernel::display::SurfaceDescriptor descriptor(kernel::display::SurfaceId id,
                                              kernel::display::DisplayTargetKind kind,
                                              kernel::display::Rect bounds)
{
    kernel::display::SurfaceDescriptor result;
    result.id = id;
    result.kind = kind;
    result.bounds = bounds;
    return result;
}

struct HitTestFixture
{
    kernel::display::DisplayTargetRegistry targets;
    kernel::display::AppSurfaceRegistry app_surfaces;
    kernel::display::GuiSurfaceRegistry gui_surfaces;

    void register_terminal_app(kernel::display::Rect bounds = {0, 0, 320, 200})
    {
        const kernel::display::AppSurface surface =
            kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                              bounds,
                                              true,
                                              true);
        ASSERT_TRUE(app_surfaces.register_surface(surface));
        ASSERT_TRUE(targets.register_target(surface.display_target()));
    }

    void register_debug_overlay()
    {
        ASSERT_TRUE(targets.register_target(descriptor(kernel::display::debug_overlay::kSurfaceId,
                                                       kernel::display::DisplayTargetKind::DebugOverlay,
                                                       {0, 0, 120, 40})));
    }

    void register_gui_surface(kernel::display::GuiSurfaceId id,
                              kernel::display::Rect bounds,
                              bool visible)
    {
        const kernel::display::GuiSurface surface =
            kernel::display::make_gui_surface(id, bounds, visible, false);
        ASSERT_TRUE(gui_surfaces.register_surface(surface));
        ASSERT_TRUE(targets.register_target(surface.display_target()));
    }
};

} // namespace

TEST(HitTestTest, ChecksRectContainment)
{
    EXPECT_TRUE(kernel::display::rect_contains({10, 20, 30, 40}, 10, 20));
    EXPECT_TRUE(kernel::display::rect_contains({10, 20, 30, 40}, 39, 59));
    EXPECT_FALSE(kernel::display::rect_contains({10, 20, 30, 40}, 40, 59));
    EXPECT_FALSE(kernel::display::rect_contains({10, 20, 30, 40}, 39, 60));
    EXPECT_FALSE(kernel::display::rect_contains({10, 20, 0, 40}, 10, 20));
}

TEST(HitTestTest, HiddenGuiPanelReturnsNoneWithoutAppSurface)
{
    HitTestFixture fixture;
    fixture.register_gui_surface(1, {10, 10, 100, 50}, false);

    const kernel::display::HitTestResult result =
        kernel::display::hit_test(fixture.targets, fixture.app_surfaces, fixture.gui_surfaces, 20, 20);

    EXPECT_FALSE(result.hit());
    EXPECT_EQ(result.target_kind, kernel::display::DisplayTargetKind::None);
    EXPECT_EQ(result.surface_id, kernel::display::kInvalidSurfaceId);
    EXPECT_EQ(result.gui_surface_id, kernel::display::kInvalidGuiSurfaceId);
}

TEST(HitTestTest, VisibleAppSurfaceHitsInsideBounds)
{
    HitTestFixture fixture;
    fixture.register_terminal_app({20, 60, 200, 100});
    fixture.register_gui_surface(1, {10, 10, 100, 40}, true);

    const kernel::display::HitTestResult result =
        kernel::display::hit_test(fixture.targets, fixture.app_surfaces, fixture.gui_surfaces, 30, 70);

    ASSERT_TRUE(result.hit());
    EXPECT_EQ(result.target_kind, kernel::display::DisplayTargetKind::AppSurface);
    EXPECT_EQ(result.surface_id,
              kernel::display::app_surface_display_id_for(kernel::display::kTerminalAppSurfaceId));
    EXPECT_EQ(result.app_surface_id, kernel::display::kTerminalAppSurfaceId);
    EXPECT_EQ(result.gui_surface_id, kernel::display::kInvalidGuiSurfaceId);
}

TEST(HitTestTest, VisibleGuiPanelHitsInsideBounds)
{
    HitTestFixture fixture;
    fixture.register_gui_surface(1, {10, 10, 100, 50}, true);

    const kernel::display::HitTestResult result =
        kernel::display::hit_test(fixture.targets, fixture.app_surfaces, fixture.gui_surfaces, 20, 20);

    ASSERT_TRUE(result.hit());
    EXPECT_EQ(result.target_kind, kernel::display::DisplayTargetKind::GuiSurface);
    EXPECT_EQ(result.surface_id, kernel::display::display_surface_id_for(1));
    EXPECT_EQ(result.gui_surface_id, 1);
}

TEST(HitTestTest, AppSurfaceWinsWhenItOverlapsGuiSurface)
{
    HitTestFixture fixture;
    fixture.register_gui_surface(1, {10, 10, 100, 50}, true);
    fixture.register_terminal_app({20, 20, 100, 50});

    const kernel::display::HitTestResult result =
        kernel::display::hit_test(fixture.targets, fixture.app_surfaces, fixture.gui_surfaces, 30, 30);

    ASSERT_TRUE(result.hit());
    EXPECT_EQ(result.target_kind, kernel::display::DisplayTargetKind::AppSurface);
    EXPECT_EQ(result.app_surface_id, kernel::display::kTerminalAppSurfaceId);
}

TEST(HitTestTest, BoundsOutsideGuiSurfaceReturnsNoneWithoutAppSurface)
{
    HitTestFixture fixture;
    fixture.register_gui_surface(1, {10, 10, 100, 50}, true);

    const kernel::display::HitTestResult result =
        kernel::display::hit_test(fixture.targets, fixture.app_surfaces, fixture.gui_surfaces, 200, 150);

    EXPECT_FALSE(result.hit());
    EXPECT_EQ(result.target_kind, kernel::display::DisplayTargetKind::None);
}

TEST(HitTestTest, UsesTopmostRegisteredVisibleGuiSurface)
{
    HitTestFixture fixture;
    fixture.register_gui_surface(1, {10, 10, 100, 50}, true);
    fixture.register_gui_surface(2, {20, 20, 100, 50}, true);

    const kernel::display::HitTestResult result =
        kernel::display::hit_test(fixture.targets, fixture.app_surfaces, fixture.gui_surfaces, 30, 30);

    ASSERT_TRUE(result.hit());
    EXPECT_EQ(result.target_kind, kernel::display::DisplayTargetKind::GuiSurface);
    EXPECT_EQ(result.gui_surface_id, 2);
}

TEST(HitTestTest, DebugOverlayDoesNotConsumePointerTarget)
{
    HitTestFixture fixture;
    fixture.register_terminal_app();
    fixture.register_debug_overlay();

    const kernel::display::HitTestResult result =
        kernel::display::hit_test(fixture.targets, fixture.app_surfaces, fixture.gui_surfaces, 20, 20);

    ASSERT_TRUE(result.hit());
    EXPECT_EQ(result.target_kind, kernel::display::DisplayTargetKind::AppSurface);
    EXPECT_EQ(result.surface_id,
              kernel::display::app_surface_display_id_for(kernel::display::kTerminalAppSurfaceId));
    EXPECT_EQ(result.app_surface_id, kernel::display::kTerminalAppSurfaceId);
}

TEST(HitTestTest, ReturnsNoneOutsideRegisteredTargets)
{
    HitTestFixture fixture;

    const kernel::display::HitTestResult result =
        kernel::display::hit_test(fixture.targets, fixture.app_surfaces, fixture.gui_surfaces, 500, 500);

    EXPECT_FALSE(result.hit());
    EXPECT_EQ(result.target_kind, kernel::display::DisplayTargetKind::None);
}
