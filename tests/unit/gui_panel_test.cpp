#include <gtest/gtest.h>

#include "kernel/display/compositor.hpp"
#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/app_surface.hpp"
#include "kernel/display/gui_panel.hpp"
#include "kernel/display/gui_panel_renderer.hpp"

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

kernel::display::GuiSurface visible_panel(kernel::display::Rect bounds = {10, 10, 80, 30})
{
    return kernel::display::make_gui_surface(kernel::display::gui_panel::kGuiSurfaceId,
                                             bounds,
                                             true,
                                             false);
}

kernel::display::gui_panel::Palette test_palette()
{
    return {{1}, {2}, {3}};
}

template <size_t Size>
void fill_pixels(uint32_t (&pixels)[Size], uint32_t value)
{
    for (uint32_t & pixel : pixels)
    {
        pixel = value;
    }
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

TEST(GuiPanelTest, VisiblePanelSitsBelowTerminalAppAndDebugOverlay)
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
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::AppSurface,
                                             kernel::display::LayerKind::DesktopPanel));
    EXPECT_TRUE(kernel::display::layer_above(kernel::display::LayerKind::DebugOverlay,
                                             kernel::display::LayerKind::AppSurface));
}

TEST(GuiPanelTest, CompositorCanTrackDesktopTerminalOverlayAndCursor)
{
    kernel::display::Compositor compositor({0, 0, 320, 200});

    ASSERT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::DesktopPanel,
                                                kernel::display::display_surface_id_for(kernel::display::gui_panel::kGuiSurfaceId))));
    ASSERT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::AppSurface,
                                                kernel::display::app_surface_display_id_for(kernel::display::kTerminalAppSurfaceId))));
    ASSERT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::DebugOverlay,
                                                kernel::display::debug_overlay::kSurfaceId)));
    ASSERT_TRUE(compositor.register_layer(layer(kernel::display::LayerKind::MouseCursor,
                                                kernel::display::kMouseCursorLayerSurfaceId)));

    EXPECT_EQ(compositor.layer_count(), 4u);
    const kernel::display::Layer * top = compositor.top_visible_layer();
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->kind, kernel::display::LayerKind::MouseCursor);
}

TEST(GuiPanelTest, ComputesRepaintRegionForVisiblePanelOnly)
{
    const kernel::display::GuiSurface panel = visible_panel({10, 10, 40, 20});

    expect_rect(kernel::display::gui_panel::repaint_region(panel, {5, 5, 20, 20}),
                10,
                10,
                15,
                15);

    kernel::display::GuiSurface hidden = panel;
    hidden.visible = false;
    EXPECT_TRUE(kernel::display::gui_panel::repaint_region(hidden, panel.bounds).empty());
    EXPECT_TRUE(kernel::display::gui_panel::repaint_region(panel, {90, 90, 2, 2}).empty());
}

TEST(GuiPanelTest, DirtyRegionPaintsTouchedBorderSegments)
{
    constexpr uint64_t width = 120;
    constexpr uint64_t height = 80;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    const kernel::display::GuiSurface panel = visible_panel({10, 10, 40, 20});
    const kernel::display::gui_panel::Palette palette = test_palette();

    kernel::display::gui_panel::paint_region(surface, panel, palette, {10, 10, 40, 1});
    EXPECT_EQ(surface.pixel(10, 10).value, 1u);
    EXPECT_EQ(surface.pixel(49, 10).value, 1u);
    EXPECT_EQ(surface.pixel(11, 11).value, 9u);

    kernel::display::gui_panel::paint_region(surface, panel, palette, {10, 10, 1, 20});
    EXPECT_EQ(surface.pixel(10, 20).value, 1u);

    kernel::display::gui_panel::paint_region(surface, panel, palette, {49, 10, 1, 20});
    EXPECT_EQ(surface.pixel(49, 20).value, 1u);

    kernel::display::gui_panel::paint_region(surface, panel, palette, {10, 29, 40, 1});
    EXPECT_EQ(surface.pixel(20, 29).value, 1u);
}

TEST(GuiPanelTest, DirtyRegionOutsidePanelDoesNotPaint)
{
    constexpr uint64_t width = 120;
    constexpr uint64_t height = 80;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    kernel::display::gui_panel::paint_region(surface,
                                             visible_panel({10, 10, 40, 20}),
                                             test_palette(),
                                             {80, 60, 4, 4});

    EXPECT_EQ(surface.pixel(10, 10).value, 9u);
    EXPECT_EQ(surface.pixel(20, 20).value, 9u);
}

TEST(GuiPanelTest, DirtyContentRegionRestoresBackgroundAndTitle)
{
    constexpr uint64_t width = 140;
    constexpr uint64_t height = 80;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    const kernel::display::GuiSurface panel = visible_panel({10, 10, 100, 40});
    const kernel::display::Rect content = kernel::display::gui_panel::content_bounds(panel.bounds);

    kernel::display::gui_panel::paint_region(surface, panel, test_palette(), content);

    bool found_foreground = false;
    bool found_background = false;
    for (uint64_t y = content.y; y < content.y + content.height; ++y)
    {
        for (uint64_t x = content.x; x < content.x + content.width; ++x)
        {
            found_foreground = found_foreground || surface.pixel(x, y).value == 3u;
            found_background = found_background || surface.pixel(x, y).value == 2u;
        }
    }

    EXPECT_TRUE(found_foreground);
    EXPECT_TRUE(found_background);
}
