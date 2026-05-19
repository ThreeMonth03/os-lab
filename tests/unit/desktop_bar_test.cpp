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
    return {{1}, {2}, {3}, {4}, {5}, {6}};
}

kernel::display::desktop_bar::TerminalItemState terminal_state(bool visible,
                                                               bool focused,
                                                               bool active,
                                                               bool closed = false)
{
    return {visible, focused, active, closed};
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
        kernel::display::desktop_bar::sample_pixel(bar, test_palette(), config, {}, 10, 64);
    const kernel::display::PixelSample background =
        kernel::display::desktop_bar::sample_pixel(bar, test_palette(), config, {}, 10, 65);
    const kernel::display::PixelSample outside =
        kernel::display::desktop_bar::sample_pixel(bar, test_palette(), config, {}, 10, 10);

    ASSERT_TRUE(border.opaque());
    EXPECT_EQ(border.color.value, 1u);
    ASSERT_TRUE(background.opaque());
    EXPECT_EQ(background.color.value, 2u);
    EXPECT_FALSE(outside.opaque());
}

TEST(DesktopBarTest, HiddenBarHasNoItemHitTarget)
{
    const kernel::display::desktop_bar::Config config{
        32,
        false,
        true,
    };
    const kernel::display::GuiSurface bar =
        kernel::display::desktop_bar::make_surface({0, 0, 1280, 720},
                                                   kernel::display::desktop_bar::kGuiSurfaceId,
                                                   config);

    const kernel::display::desktop_bar::Item item =
        kernel::display::desktop_bar::terminal_item_for(bar, config, terminal_state(false, false, false));
    const kernel::display::desktop_bar::HitTestResult hit =
        kernel::display::desktop_bar::hit_test(bar, config, terminal_state(false, false, false), 10, 700);

    EXPECT_FALSE(item.valid());
    EXPECT_EQ(hit.region, kernel::display::desktop_bar::HitRegion::None);
    EXPECT_FALSE(hit.hit_item());
}

TEST(DesktopBarTest, VisibleDebugActionsBarComputesTerminalItem)
{
    const kernel::display::desktop_bar::Config config{
        32,
        true,
        true,
    };
    const kernel::display::GuiSurface bar =
        kernel::display::desktop_bar::make_surface({0, 0, 1280, 720},
                                                   kernel::display::desktop_bar::kGuiSurfaceId,
                                                   config);

    const kernel::display::desktop_bar::Item hidden_terminal_item =
        kernel::display::desktop_bar::terminal_item_for(bar, config, terminal_state(false, false, false));
    const kernel::display::desktop_bar::Item visible_terminal_item =
        kernel::display::desktop_bar::terminal_item_for(bar, config, terminal_state(true, false, false));

    ASSERT_TRUE(hidden_terminal_item.valid());
    EXPECT_EQ(hidden_terminal_item.kind, kernel::display::desktop_bar::ItemKind::Terminal);
    EXPECT_EQ(hidden_terminal_item.action,
              kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus);
    EXPECT_TRUE(hidden_terminal_item.enabled);
    EXPECT_FALSE(hidden_terminal_item.active);
    EXPECT_FALSE(hidden_terminal_item.focused);
    expect_rect(hidden_terminal_item.bounds, 6, 694, 96, 20);

    ASSERT_TRUE(visible_terminal_item.valid());
    EXPECT_TRUE(visible_terminal_item.enabled);
    EXPECT_FALSE(visible_terminal_item.active);
    EXPECT_FALSE(visible_terminal_item.focused);

    const kernel::display::desktop_bar::Item active_terminal_item =
        kernel::display::desktop_bar::terminal_item_for(bar, config, terminal_state(true, false, true));
    ASSERT_TRUE(active_terminal_item.valid());
    EXPECT_TRUE(active_terminal_item.enabled);
    EXPECT_TRUE(active_terminal_item.active);
    EXPECT_FALSE(active_terminal_item.focused);

    const kernel::display::desktop_bar::Item focused_terminal_item =
        kernel::display::desktop_bar::terminal_item_for(bar, config, terminal_state(true, true, true));
    ASSERT_TRUE(focused_terminal_item.valid());
    EXPECT_FALSE(focused_terminal_item.enabled);
    EXPECT_TRUE(focused_terminal_item.active);
    EXPECT_TRUE(focused_terminal_item.focused);

    const kernel::display::desktop_bar::Item closed_terminal_item =
        kernel::display::desktop_bar::terminal_item_for(bar, config, terminal_state(false, false, false, true));
    ASSERT_TRUE(closed_terminal_item.valid());
    EXPECT_FALSE(closed_terminal_item.enabled);
    EXPECT_FALSE(closed_terminal_item.active);
    EXPECT_FALSE(closed_terminal_item.focused);
}

TEST(DesktopBarTest, VisibleBarWithoutDebugActionsHasNoActionableItem)
{
    const kernel::display::desktop_bar::Config config{
        32,
        true,
        false,
    };
    const kernel::display::GuiSurface bar =
        kernel::display::desktop_bar::make_surface({0, 0, 1280, 720},
                                                   kernel::display::desktop_bar::kGuiSurfaceId,
                                                   config);

    const kernel::display::desktop_bar::Item item =
        kernel::display::desktop_bar::terminal_item_for(bar, config, terminal_state(false, false, false));
    const kernel::display::desktop_bar::HitTestResult hit =
        kernel::display::desktop_bar::hit_test(bar, config, terminal_state(false, false, false), 12, 700);

    EXPECT_FALSE(item.valid());
    EXPECT_EQ(hit.region, kernel::display::desktop_bar::HitRegion::Background);
    EXPECT_FALSE(hit.hit_item());
}

TEST(DesktopBarTest, TerminalItemHitTestDistinguishesItemAndBackground)
{
    const kernel::display::desktop_bar::Config config{
        32,
        true,
        true,
    };
    const kernel::display::GuiSurface bar =
        kernel::display::desktop_bar::make_surface({0, 0, 1280, 720},
                                                   kernel::display::desktop_bar::kGuiSurfaceId,
                                                   config);

    const kernel::display::desktop_bar::HitTestResult item =
        kernel::display::desktop_bar::hit_test(bar, config, terminal_state(false, false, false), 12, 700);
    const kernel::display::desktop_bar::HitTestResult background =
        kernel::display::desktop_bar::hit_test(bar, config, terminal_state(false, false, false), 200, 700);
    const kernel::display::desktop_bar::HitTestResult outside =
        kernel::display::desktop_bar::hit_test(bar, config, terminal_state(false, false, false), 12, 650);

    EXPECT_EQ(item.region, kernel::display::desktop_bar::HitRegion::Item);
    EXPECT_EQ(item.item_kind, kernel::display::desktop_bar::ItemKind::Terminal);
    EXPECT_EQ(item.action, kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus);
    EXPECT_TRUE(item.item_enabled);
    EXPECT_EQ(background.region, kernel::display::desktop_bar::HitRegion::Background);
    EXPECT_EQ(background.item_kind, kernel::display::desktop_bar::ItemKind::None);
    EXPECT_EQ(outside.region, kernel::display::desktop_bar::HitRegion::None);
}

TEST(DesktopBarTest, TerminalItemPixelSamplingDrawsAffordance)
{
    const kernel::display::desktop_bar::Config config{
        32,
        true,
        true,
    };
    const kernel::display::GuiSurface bar =
        kernel::display::desktop_bar::make_surface({0, 0, 1280, 720},
                                                   kernel::display::desktop_bar::kGuiSurfaceId,
                                                   config);

    const kernel::display::PixelSample button_border =
        kernel::display::desktop_bar::sample_pixel(bar,
                                                   test_palette(),
                                                   config,
                                                   terminal_state(false, false, false),
                                                   6,
                                                   694);
    const kernel::display::PixelSample button_fill =
        kernel::display::desktop_bar::sample_pixel(bar,
                                                   test_palette(),
                                                   config,
                                                   terminal_state(false, false, false),
                                                   30,
                                                   700);
    const kernel::display::PixelSample disabled_fill =
        kernel::display::desktop_bar::sample_pixel(bar,
                                                   test_palette(),
                                                   config,
                                                   terminal_state(true, true, true),
                                                   30,
                                                   700);

    ASSERT_TRUE(button_border.opaque());
    EXPECT_EQ(button_border.color.value, 3u);
    ASSERT_TRUE(button_fill.opaque());
    EXPECT_EQ(button_fill.color.value, 4u);
    ASSERT_TRUE(disabled_fill.opaque());
    EXPECT_EQ(disabled_fill.color.value, 5u);
}

TEST(DesktopBarTest, DefaultDebugOverlayPlacementDoesNotOverlapBottomBar)
{
    const kernel::display::desktop_bar::Layout layout =
        kernel::display::desktop_bar::layout_for({0, 0, 1280, 720}, {32, true});

    const kernel::display::Rect overlay =
        kernel::display::debug_overlay::bounds_for(1280, 720, kernel::display::debug_overlay::Config{});

    EXPECT_TRUE(kernel::display::intersect_rect(overlay, layout.bar_bounds).empty());
}
