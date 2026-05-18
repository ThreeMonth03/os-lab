#include <gtest/gtest.h>

#include "kernel/display/app_layout.hpp"

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

TEST(AppLayoutTest, HiddenPanelKeepsTerminalAwayFromFramebufferTopEdge)
{
    const kernel::display::Rect bounds =
        kernel::display::DesktopAppLayout::primary_app_bounds_for({
            {0, 0, 1280, 760},
            {},
            false,
            16,
            16,
            8,
            16,
        });

    expect_rect(bounds, 0, 16, 1280, 744);
}

TEST(AppLayoutTest, HiddenPanelPreservesAtLeastOneTextCellOnShortFramebuffers)
{
    const kernel::display::Rect bounds =
        kernel::display::DesktopAppLayout::primary_app_bounds_for({
            {0, 0, 80, 16},
            {},
            false,
            16,
            16,
            8,
            16,
        });

    expect_rect(bounds, 0, 0, 80, 16);
}

TEST(AppLayoutTest, VisiblePanelPlacesTerminalBelowPanel)
{
    const kernel::display::Rect bounds =
        kernel::display::DesktopAppLayout::primary_app_bounds_for({
            {0, 0, 1280, 760},
            {16, 16, 180, 56},
            true,
            16,
            16,
            8,
            16,
        });

    expect_rect(bounds, 16, 88, 1248, 656);
}

TEST(AppLayoutTest, InvalidPanelLayoutFallsBackToSafeHiddenPanelBounds)
{
    const kernel::display::Rect bounds =
        kernel::display::DesktopAppLayout::primary_app_bounds_for({
            {0, 0, 1280, 760},
            {16, 730, 180, 56},
            true,
            16,
            16,
            8,
            16,
        });

    expect_rect(bounds, 0, 16, 1280, 744);
}

TEST(AppLayoutTest, RejectsBoundsThatCannotFitATextCell)
{
    const kernel::display::Rect bounds =
        kernel::display::DesktopAppLayout::primary_app_bounds_for({
            {0, 0, 7, 15},
            {},
            false,
            16,
            16,
            8,
            16,
        });

    EXPECT_TRUE(bounds.empty());
}

TEST(AppLayoutTest, ComputesAppCellCapacityFromBounds)
{
    const kernel::display::AppCellCapacity capacity =
        kernel::display::DesktopAppLayout::cell_capacity_for({16, 32, 1280, 744}, 8, 16);

    EXPECT_TRUE(capacity.valid());
    EXPECT_EQ(capacity.columns, 160u);
    EXPECT_EQ(capacity.rows, 46u);
}

TEST(AppLayoutTest, RejectsInvalidAppCellCapacity)
{
    EXPECT_FALSE(kernel::display::DesktopAppLayout::cell_capacity_for({0, 0, 7, 16}, 8, 16).valid());
    EXPECT_FALSE(kernel::display::DesktopAppLayout::cell_capacity_for({0, 0, 8, 15}, 8, 16).valid());
    EXPECT_FALSE(kernel::display::DesktopAppLayout::cell_capacity_for({0, 0, 8, 16}, 0, 16).valid());
}
