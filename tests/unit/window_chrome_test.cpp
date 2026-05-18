#include <gtest/gtest.h>

#include "kernel/display/app_layout.hpp"
#include "kernel/display/terminal_renderer.hpp"
#include "kernel/display/window_chrome.hpp"

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

} // namespace

TEST(WindowChromeTest, HiddenChromeUsesOuterBoundsAsClientBounds)
{
    kernel::display::WindowFrameConfig config;
    config.visible = false;

    const kernel::display::WindowFrameMetrics metrics =
        kernel::display::WindowChrome::metrics_for({10, 20, 320, 200}, config);

    ASSERT_TRUE(metrics.valid());
    EXPECT_FALSE(metrics.visible);
    expect_rect(metrics.outer_bounds, 10, 20, 320, 200);
    expect_rect(metrics.client_bounds, 10, 20, 320, 200);
    EXPECT_TRUE(metrics.title_bar_bounds.empty());
    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 20, 30),
              kernel::display::WindowChromeHitRegion::Content);
}

TEST(WindowChromeTest, VisibleChromeDerivesTitleBarClientAndControlBounds)
{
    kernel::display::WindowFrameConfig config;
    config.visible = true;
    config.border_thickness = 2;
    config.title_bar_height = 24;
    config.close_button_size = 12;
    config.chrome_margin = 4;
    config.resize_handle_size = 14;

    const kernel::display::WindowFrameMetrics metrics =
        kernel::display::WindowChrome::metrics_for({10, 20, 320, 200}, config);

    ASSERT_TRUE(metrics.valid());
    EXPECT_TRUE(metrics.visible);
    expect_rect(metrics.title_bar_bounds, 10, 20, 320, 24);
    expect_rect(metrics.client_bounds, 12, 44, 316, 174);
    expect_rect(metrics.close_button_bounds, 314, 26, 12, 12);
    expect_rect(metrics.resize_handle_bounds, 316, 206, 14, 14);
}

TEST(WindowChromeTest, RejectsVisibleChromeThatCannotFitClientBounds)
{
    kernel::display::WindowFrameConfig config;
    config.visible = true;
    config.border_thickness = 2;
    config.title_bar_height = 24;

    const kernel::display::WindowFrameMetrics metrics =
        kernel::display::WindowChrome::metrics_for({0, 0, 20, 24}, config);

    EXPECT_FALSE(metrics.valid());
    EXPECT_TRUE(metrics.client_bounds.empty());
}

TEST(WindowChromeTest, ClientBoundsCanBeUsedForTerminalCellCapacity)
{
    kernel::display::WindowFrameConfig config;
    config.visible = true;

    const kernel::display::WindowFrameMetrics metrics =
        kernel::display::WindowChrome::metrics_for({0, 0, 1280, 744}, config);
    const kernel::display::AppCellCapacity capacity =
        kernel::display::DesktopAppLayout::cell_capacity_for(
            metrics.client_bounds,
            kernel::display::TerminalRenderer::kCellWidth,
            kernel::display::TerminalRenderer::kCellHeight);

    ASSERT_TRUE(capacity.valid());
    EXPECT_EQ(capacity.columns, 106u);
    EXPECT_EQ(capacity.rows, 40u);
}

TEST(WindowChromeTest, HitTestPrioritizesControlsAndContent)
{
    kernel::display::WindowFrameConfig config;
    config.visible = true;
    const kernel::display::WindowFrameMetrics metrics =
        kernel::display::WindowChrome::metrics_for({10, 20, 320, 200}, config);

    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 318, 30),
              kernel::display::WindowChromeHitRegion::CloseButton);
    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 325, 215),
              kernel::display::WindowChromeHitRegion::ResizeBottomRight);
    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 10, 100),
              kernel::display::WindowChromeHitRegion::ResizeLeft);
    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 329, 100),
              kernel::display::WindowChromeHitRegion::ResizeRight);
    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 200, 219),
              kernel::display::WindowChromeHitRegion::ResizeBottom);
    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 200, 20),
              kernel::display::WindowChromeHitRegion::ResizeTop);
    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 40, 30),
              kernel::display::WindowChromeHitRegion::TitleBar);
    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 40, 50),
              kernel::display::WindowChromeHitRegion::Content);
    EXPECT_EQ(kernel::display::WindowChrome::hit_test(metrics, 400, 400),
              kernel::display::WindowChromeHitRegion::None);
}

TEST(WindowChromeTest, CloseButtonIconDrawsAnXInsideButtonBounds)
{
    kernel::display::WindowFrameConfig config;
    config.visible = true;
    const kernel::display::WindowFrameMetrics metrics =
        kernel::display::WindowChrome::metrics_for({10, 20, 320, 200}, config);

    const kernel::display::Rect close = metrics.close_button_bounds;
    ASSERT_FALSE(close.empty());

    EXPECT_TRUE(kernel::display::WindowChrome::close_button_icon_contains_pixel(
        metrics,
        close.x + 2,
        close.y + 2));
    EXPECT_TRUE(kernel::display::WindowChrome::close_button_icon_contains_pixel(
        metrics,
        close.x + close.width - 3,
        close.y + 2));
    EXPECT_TRUE(kernel::display::WindowChrome::close_button_icon_contains_pixel(
        metrics,
        close.x + 2,
        close.y + close.height - 3));
    EXPECT_TRUE(kernel::display::WindowChrome::close_button_icon_contains_pixel(
        metrics,
        close.x + close.width - 3,
        close.y + close.height - 3));

    EXPECT_FALSE(kernel::display::WindowChrome::close_button_icon_contains_pixel(
        metrics,
        close.x,
        close.y));
    EXPECT_FALSE(kernel::display::WindowChrome::close_button_icon_contains_pixel(
        metrics,
        close.x + (close.width / 2),
        close.y + 2));
}
