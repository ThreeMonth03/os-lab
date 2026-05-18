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

} // namespace

TEST(WindowInteractionTest, ResizeDragGrowsFromAnchor)
{
    const kernel::display::WindowResizeDrag drag =
        kernel::display::WindowResizeDrag::begin({10, 20, 320, 200}, 329, 219, constraints());

    ASSERT_TRUE(drag.valid());
    expect_rect(drag.bounds_for(429, 269), 10, 20, 420, 250);
}

TEST(WindowInteractionTest, ResizeDragClampsToWorkArea)
{
    const kernel::display::WindowResizeDrag drag =
        kernel::display::WindowResizeDrag::begin({10, 20, 320, 200}, 329, 219, constraints());

    expect_rect(drag.bounds_for(2000, 2000), 10, 20, 790, 580);
}

TEST(WindowInteractionTest, ResizeDragRejectsTooSmallBounds)
{
    const kernel::display::WindowResizeConstraints tight_constraints{
        {0, 0, 100, 80},
        120,
        90,
        kernel::display::terminal_window_frame_config(true),
    };
    const kernel::display::WindowResizeDrag drag =
        kernel::display::WindowResizeDrag::begin({10, 20, 80, 40}, 89, 59, tight_constraints);

    EXPECT_TRUE(drag.bounds_for(60, 40).empty());
}

TEST(WindowInteractionTest, ResizeDragKeepsMinimumClientCapacity)
{
    const kernel::display::WindowResizeDrag drag =
        kernel::display::WindowResizeDrag::begin({10, 20, 320, 200}, 329, 219, constraints());

    const kernel::display::Rect bounds = drag.bounds_for(20, 30);
    const kernel::display::WindowFrameMetrics metrics =
        kernel::display::WindowChrome::metrics_for(
            bounds,
            kernel::display::terminal_window_frame_config(true));

    ASSERT_TRUE(metrics.valid());
    EXPECT_GE(metrics.client_bounds.width, 120u);
    EXPECT_GE(metrics.client_bounds.height, 90u);
}
