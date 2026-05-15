#include <gtest/gtest.h>

#include "kernel/display/compositor.hpp"
#include "kernel/display/debug_overlay.hpp"
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

kernel::display::Layer layer(kernel::display::LayerKind kind, kernel::display::SurfaceId id)
{
    return {kind, id, {0, 0, 320, 200}, true};
}

} // namespace

TEST(GuiPanelTest, ComputesDefaultPanelBounds)
{
    const kernel::display::Rect bounds = kernel::display::gui_panel::bounds_for(1280, 720);

    expect_rect(bounds,
                kernel::display::gui_panel::kDefaultMargin,
                kernel::display::gui_panel::kDefaultMargin,
                kernel::display::gui_panel::kDefaultWidth,
                kernel::display::gui_panel::kDefaultHeight);
}

TEST(GuiPanelTest, DefaultConfigKeepsPanelHidden)
{
    const kernel::display::gui_panel::Config config = kernel::display::gui_panel::default_config();
    const kernel::display::GuiSurface panel =
        kernel::display::gui_panel::make_surface(1280, 720, kernel::display::gui_panel::kGuiSurfaceId, config);

    EXPECT_FALSE(config.visible);
    EXPECT_FALSE(panel.visible);
    EXPECT_FALSE(kernel::display::gui_panel::should_redraw(panel));
}

TEST(GuiPanelTest, ClampsPanelBoundsToAvailableSurface)
{
    const kernel::display::gui_panel::Config config{
        400,
        300,
        8,
        true,
    };
    const kernel::display::Rect bounds = kernel::display::gui_panel::bounds_for(100, 70, config);

    expect_rect(bounds, 8, 8, 84, 54);
}

TEST(GuiPanelTest, ComputesContentBounds)
{
    const kernel::display::Rect content =
        kernel::display::gui_panel::content_bounds({10, 20, 100, 50}, 6);

    expect_rect(content, 16, 26, 88, 38);
    EXPECT_TRUE(kernel::display::gui_panel::content_bounds({10, 20, 10, 50}, 6).empty());
}

TEST(GuiPanelTest, HiddenPanelDoesNotRequestRedraw)
{
    const kernel::display::GuiSurface panel = kernel::display::gui_panel::make_surface(1280, 720);

    EXPECT_TRUE(panel.valid());
    EXPECT_FALSE(panel.visible);
    EXPECT_FALSE(panel.focusable);
    EXPECT_FALSE(kernel::display::gui_panel::should_redraw(panel));
    EXPECT_FALSE(panel.layer().visible);
}

TEST(GuiPanelTest, DebugVisibleConfigRequestsRedraw)
{
    kernel::display::gui_panel::Config config = kernel::display::gui_panel::default_config();
    config.visible = true;

    const kernel::display::GuiSurface panel =
        kernel::display::gui_panel::make_surface(1280, 720, kernel::display::gui_panel::kGuiSurfaceId, config);

    EXPECT_TRUE(panel.visible);
    EXPECT_TRUE(kernel::display::gui_panel::should_redraw(panel));
}

TEST(GuiPanelTest, VisiblePanelSitsBetweenConsoleAndDebugOverlay)
{
    const kernel::display::gui_panel::Config visible_config{
        kernel::display::gui_panel::kDefaultWidth,
        kernel::display::gui_panel::kDefaultHeight,
        kernel::display::gui_panel::kDefaultMargin,
        true,
    };
    const kernel::display::GuiSurface panel =
        kernel::display::gui_panel::make_surface(1280, 720, kernel::display::gui_panel::kGuiSurfaceId, visible_config);

    EXPECT_TRUE(kernel::display::gui_panel::should_redraw(panel));
    EXPECT_TRUE(panel.layer().visible);
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::GuiSurface,
                                             kernel::display::LayerKind::Console));
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::DebugOverlay,
                                             kernel::display::LayerKind::GuiSurface));
}

TEST(GuiPanelTest, CompositorCanTrackConsolePanelOverlayAndCursor)
{
    kernel::display::Compositor compositor({0, 0, 320, 200});

    ASSERT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::Console,
                                                kernel::display::kConsoleSurfaceId)));
    ASSERT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::GuiSurface,
                                                kernel::display::display_surface_id_for(kernel::display::gui_panel::kGuiSurfaceId))));
    ASSERT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::DebugOverlay,
                                                kernel::display::debug_overlay::kSurfaceId)));
    ASSERT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::MouseCursor,
                                                kernel::display::kMouseCursorLayerSurfaceId)));

    EXPECT_EQ(compositor.layer_count(), 4u);
    const kernel::display::Layer * top = compositor.top_visible_layer();
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->kind, kernel::display::LayerKind::MouseCursor);
}
