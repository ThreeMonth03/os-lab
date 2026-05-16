#include <gtest/gtest.h>

#include "kernel/display/composited_surface.hpp"
#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/desktop_background.hpp"
#include "kernel/display/gui_panel.hpp"

namespace
{

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

} // namespace

TEST(CompositedSurfaceTest, MapsRolesToLayerOrder)
{
    EXPECT_EQ(kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::Background),
              kernel::display::LayerKind::DesktopBackground);
    EXPECT_EQ(kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::SystemUi),
              kernel::display::LayerKind::GuiSurface);
    EXPECT_EQ(kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::App),
              kernel::display::LayerKind::AppSurface);
    EXPECT_EQ(kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::Overlay),
              kernel::display::LayerKind::DebugOverlay);
    EXPECT_EQ(kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::Cursor),
              kernel::display::LayerKind::MouseCursor);

    EXPECT_TRUE(kernel::display::layer_above(
        kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::SystemUi),
        kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::Background)));
    EXPECT_TRUE(kernel::display::layer_above(
        kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::App),
        kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::SystemUi)));
    EXPECT_TRUE(kernel::display::layer_above(
        kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::Overlay),
        kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::App)));
    EXPECT_TRUE(kernel::display::layer_above(
        kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::Cursor),
        kernel::display::layer_kind_for(kernel::display::CompositedSurfaceRole::Overlay)));
}

TEST(CompositedSurfaceTest, MapsRolesToInputPolicy)
{
    EXPECT_EQ(kernel::display::default_input_policy_for(kernel::display::CompositedSurfaceRole::Background),
              kernel::display::SurfaceInputPolicy::None);
    EXPECT_EQ(kernel::display::default_input_policy_for(kernel::display::CompositedSurfaceRole::SystemUi),
              kernel::display::SurfaceInputPolicy::None);
    EXPECT_EQ(kernel::display::default_input_policy_for(kernel::display::CompositedSurfaceRole::App),
              kernel::display::SurfaceInputPolicy::KeyboardFocus);
    EXPECT_EQ(kernel::display::default_input_policy_for(kernel::display::CompositedSurfaceRole::Overlay),
              kernel::display::SurfaceInputPolicy::None);
    EXPECT_EQ(kernel::display::default_input_policy_for(kernel::display::CompositedSurfaceRole::Cursor),
              kernel::display::SurfaceInputPolicy::None);
}

TEST(CompositedSurfaceTest, BuildsTerminalAppDescriptor)
{
    const kernel::display::CompositedSurfaceDescriptor terminal =
        kernel::display::make_composited_surface(201,
                                                 kernel::display::CompositedSurfaceRole::App,
                                                 {8, 16, 320, 200},
                                                 true,
                                                 true,
                                                 true);

    ASSERT_TRUE(terminal.valid());
    EXPECT_TRUE(terminal.accepts_keyboard_focus());
    EXPECT_EQ(terminal.occlusion, kernel::display::LayerOcclusion::Opaque);

    const kernel::display::Layer layer = terminal.layer();
    EXPECT_EQ(layer.kind, kernel::display::LayerKind::AppSurface);
    EXPECT_TRUE(layer.occludes_lower_repaint());
    expect_rect(layer.bounds, 8, 16, 320, 200);

    const kernel::display::SurfaceDescriptor target = terminal.display_target();
    EXPECT_EQ(target.kind, kernel::display::DisplayTargetKind::AppSurface);
    EXPECT_TRUE(target.active);
    EXPECT_TRUE(target.focused);
}

TEST(CompositedSurfaceTest, BuildsSystemUiPanelDescriptor)
{
    const kernel::display::CompositedSurfaceDescriptor panel =
        kernel::display::make_composited_surface(kernel::display::display_surface_id_for(
                                                     kernel::display::gui_panel::kGuiSurfaceId),
                                                 kernel::display::CompositedSurfaceRole::SystemUi,
                                                 {10, 10, 180, 56},
                                                 true,
                                                 false,
                                                 false);

    ASSERT_TRUE(panel.valid());
    EXPECT_FALSE(panel.accepts_keyboard_focus());
    EXPECT_EQ(panel.occlusion, kernel::display::LayerOcclusion::Opaque);
    EXPECT_EQ(panel.layer().kind, kernel::display::LayerKind::GuiSurface);
    EXPECT_EQ(panel.display_target().kind, kernel::display::DisplayTargetKind::GuiSurface);
}

TEST(CompositedSurfaceTest, BuildsOverlayDescriptor)
{
    const kernel::display::CompositedSurfaceDescriptor overlay =
        kernel::display::make_composited_surface(kernel::display::debug_overlay::kSurfaceId,
                                                 kernel::display::CompositedSurfaceRole::Overlay,
                                                 {700, 0, 80, 20});

    ASSERT_TRUE(overlay.valid());
    EXPECT_FALSE(overlay.accepts_keyboard_focus());
    EXPECT_EQ(overlay.occlusion, kernel::display::LayerOcclusion::Opaque);
    EXPECT_EQ(overlay.layer().kind, kernel::display::LayerKind::DebugOverlay);
    EXPECT_EQ(overlay.display_target().kind, kernel::display::DisplayTargetKind::DebugOverlay);
}

TEST(CompositedSurfaceTest, BuildsCursorDescriptorAsTransparentTopLayer)
{
    const kernel::display::CompositedSurfaceDescriptor cursor =
        kernel::display::make_composited_surface(kernel::display::kMouseCursorLayerSurfaceId,
                                                 kernel::display::CompositedSurfaceRole::Cursor,
                                                 {0, 0, 800, 600});

    ASSERT_TRUE(cursor.valid());
    EXPECT_FALSE(cursor.accepts_keyboard_focus());
    EXPECT_EQ(cursor.occlusion, kernel::display::LayerOcclusion::Transparent);
    EXPECT_EQ(cursor.layer().kind, kernel::display::LayerKind::MouseCursor);
    EXPECT_FALSE(cursor.layer().occludes_lower_repaint());
    EXPECT_FALSE(cursor.display_target().valid());
    EXPECT_TRUE(kernel::display::layer_above(cursor.layer().kind, kernel::display::LayerKind::DebugOverlay));
}

TEST(CompositedSurfaceTest, RejectsInvalidOrUnfocusableFocusedSurface)
{
    EXPECT_FALSE(kernel::display::make_composited_surface(kernel::display::kInvalidSurfaceId,
                                                          kernel::display::CompositedSurfaceRole::App,
                                                          {0, 0, 10, 10})
                     .valid());
    EXPECT_FALSE(kernel::display::make_composited_surface(42,
                                                          kernel::display::CompositedSurfaceRole::None,
                                                          {0, 0, 10, 10})
                     .valid());
    EXPECT_FALSE(kernel::display::make_composited_surface(42,
                                                          kernel::display::CompositedSurfaceRole::Overlay,
                                                          {0, 0, 10, 10},
                                                          true,
                                                          false,
                                                          true)
                     .valid());
}

TEST(CompositedSurfaceTest, BackgroundAndCursorAreNotDisplayTargets)
{
    const kernel::display::CompositedSurfaceDescriptor background =
        kernel::display::make_composited_surface(kernel::display::desktop_background::kSurfaceId,
                                                 kernel::display::CompositedSurfaceRole::Background,
                                                 {0, 0, 800, 600});

    ASSERT_TRUE(background.valid());
    EXPECT_EQ(background.occlusion, kernel::display::LayerOcclusion::Opaque);
    EXPECT_EQ(background.layer().kind, kernel::display::LayerKind::DesktopBackground);
    EXPECT_FALSE(background.display_target().valid());

    EXPECT_FALSE(kernel::display::make_composited_surface(kernel::display::desktop_background::kSurfaceId,
                                                          kernel::display::CompositedSurfaceRole::Background,
                                                          {0, 0, 800, 600},
                                                          true,
                                                          true,
                                                          false)
                     .valid());
}
