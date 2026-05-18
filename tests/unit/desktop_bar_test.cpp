#include <gtest/gtest.h>

#include "kernel/display/composited_surface.hpp"
#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/desktop_bar.hpp"

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

kernel::display::desktop_bar::Palette test_palette()
{
    return {{1}, {2}};
}

} // namespace

TEST(DesktopBarTest, DefaultConfigKeepsBarHidden)
{
    const kernel::display::desktop_bar::Config config =
        kernel::display::desktop_bar::default_config();
    const kernel::display::GuiSurface bar =
        kernel::display::desktop_bar::make_surface({0, 0, 1280, 720},
                                                   kernel::display::desktop_bar::kGuiSurfaceId,
                                                   config);

    EXPECT_FALSE(config.visible);
    EXPECT_FALSE(bar.visible);
    EXPECT_FALSE(kernel::display::desktop_bar::should_redraw(bar));
}

TEST(DesktopBarTest, PlacesVisibleBarAtBottomAndComputesWorkArea)
{
    const kernel::display::desktop_bar::Config config{
        32,
        true,
    };

    const kernel::display::desktop_bar::Layout layout =
        kernel::display::desktop_bar::layout_for({0, 0, 1280, 720}, config);

    EXPECT_TRUE(layout.bar_visible);
    expect_rect(layout.bar_bounds, 0, 688, 1280, 32);
    expect_rect(layout.work_area, 0, 0, 1280, 688);
}

TEST(DesktopBarTest, HiddenBarKeepsFullWorkArea)
{
    const kernel::display::desktop_bar::Config config{
        32,
        false,
    };

    const kernel::display::desktop_bar::Layout layout =
        kernel::display::desktop_bar::layout_for({0, 0, 1280, 720}, config);

    EXPECT_FALSE(layout.bar_visible);
    expect_rect(layout.bar_bounds, 0, 688, 1280, 32);
    expect_rect(layout.work_area, 0, 0, 1280, 720);
}

TEST(DesktopBarTest, ClampsBarHeightToSmallFramebuffer)
{
    const kernel::display::desktop_bar::Config config{
        32,
        true,
    };

    const kernel::display::desktop_bar::Layout layout =
        kernel::display::desktop_bar::layout_for({0, 0, 80, 20}, config);

    EXPECT_TRUE(layout.bar_visible);
    expect_rect(layout.bar_bounds, 0, 0, 80, 20);
    EXPECT_TRUE(layout.work_area.empty());
}

TEST(DesktopBarTest, RejectsTooSmallBar)
{
    const kernel::display::desktop_bar::Config config{
        4,
        true,
    };

    const kernel::display::desktop_bar::Layout layout =
        kernel::display::desktop_bar::layout_for({0, 0, 80, 20}, config);

    EXPECT_FALSE(layout.bar_visible);
    EXPECT_TRUE(layout.bar_bounds.empty());
    expect_rect(layout.work_area, 0, 0, 80, 20);
}

TEST(DesktopBarTest, BuildsSystemUiNonFocusableSurface)
{
    const kernel::display::desktop_bar::Config config{
        32,
        true,
    };

    const kernel::display::GuiSurface bar =
        kernel::display::desktop_bar::make_surface({0, 0, 1280, 720},
                                                   kernel::display::desktop_bar::kGuiSurfaceId,
                                                   config);

    ASSERT_TRUE(bar.valid());
    EXPECT_TRUE(bar.visible);
    EXPECT_FALSE(bar.focusable);
    EXPECT_EQ(bar.composited_surface().role, kernel::display::CompositedSurfaceRole::SystemUi);
    EXPECT_EQ(bar.composited_surface().input_policy, kernel::display::SurfaceInputPolicy::None);
    EXPECT_EQ(bar.composited_surface().layer().kind, kernel::display::LayerKind::GuiSurface);
}

TEST(DesktopBarTest, SamplesBorderAndBackgroundPixels)
{
    const kernel::display::desktop_bar::Config config{
        16,
        true,
    };
    const kernel::display::GuiSurface bar =
        kernel::display::desktop_bar::make_surface({0, 0, 100, 80},
                                                   kernel::display::desktop_bar::kGuiSurfaceId,
                                                   config);

    const kernel::display::PixelSample border =
        kernel::display::desktop_bar::sample_pixel(bar, test_palette(), 10, 64);
    const kernel::display::PixelSample background =
        kernel::display::desktop_bar::sample_pixel(bar, test_palette(), 10, 65);
    const kernel::display::PixelSample outside =
        kernel::display::desktop_bar::sample_pixel(bar, test_palette(), 10, 10);

    ASSERT_TRUE(border.opaque());
    EXPECT_EQ(border.color.value, 1u);
    ASSERT_TRUE(background.opaque());
    EXPECT_EQ(background.color.value, 2u);
    EXPECT_FALSE(outside.opaque());
}

TEST(DesktopBarTest, DefaultDebugOverlayPlacementDoesNotOverlapBottomBar)
{
    const kernel::display::desktop_bar::Layout layout =
        kernel::display::desktop_bar::layout_for({0, 0, 1280, 720}, {32, true});

    const kernel::display::Rect overlay =
        kernel::display::debug_overlay::bounds_for(1280, 720, kernel::display::debug_overlay::Config{});

    EXPECT_TRUE(kernel::display::intersect_rect(overlay, layout.bar_bounds).empty());
}
