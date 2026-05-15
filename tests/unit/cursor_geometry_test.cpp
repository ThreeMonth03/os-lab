#include <gtest/gtest.h>

#include "kernel/display/cursor_geometry.hpp"

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

TEST(CursorGeometryTest, GivesFullBitmapRectWhenInsideSurface)
{
    const kernel::display::CursorGeometry geometry({0, 0, 100, 50}, 10, 16);

    expect_rect(geometry.visible_rect(20, 10), 20, 10, 10, 16);
}

TEST(CursorGeometryTest, ClipsVisibleRectAtRightAndBottomEdges)
{
    const kernel::display::CursorGeometry geometry({0, 0, 100, 50}, 10, 16);

    expect_rect(geometry.visible_rect(95, 45), 95, 45, 5, 5);
    expect_rect(geometry.visible_rect(99, 49), 99, 49, 1, 1);
}

TEST(CursorGeometryTest, KeepsDamageRectCursorSizedAtRightAndBottomEdges)
{
    const kernel::display::CursorGeometry geometry({0, 0, 100, 50}, 10, 16);

    expect_rect(geometry.damage_rect(95, 45), 90, 34, 10, 16);
    expect_rect(geometry.damage_rect(99, 49), 90, 34, 10, 16);
}

TEST(CursorGeometryTest, ClampsDamageRectWhenBitmapIsLargerThanSurface)
{
    const kernel::display::CursorGeometry geometry({0, 0, 8, 8}, 10, 16);

    expect_rect(geometry.damage_rect(7, 7), 0, 0, 8, 8);
}

TEST(CursorGeometryTest, ClipsVisibleRectToNonZeroSurfaceOrigin)
{
    const kernel::display::CursorGeometry geometry({10, 20, 100, 50}, 10, 16);

    expect_rect(geometry.visible_rect(105, 65), 105, 65, 5, 5);
    expect_rect(geometry.damage_rect(105, 65), 100, 54, 10, 16);
    EXPECT_TRUE(geometry.visible_rect(0, 0).empty());
}

TEST(CursorGeometryTest, DetectsEdgePushAtHotspotBounds)
{
    const kernel::display::CursorGeometry geometry({0, 0, 100, 50}, 10, 16);

    EXPECT_TRUE(geometry.edge_push(99, 20, 1, 0));
    EXPECT_TRUE(geometry.edge_push(50, 49, 0, 1));
    EXPECT_TRUE(geometry.edge_push(0, 20, -1, 0));
    EXPECT_TRUE(geometry.edge_push(50, 0, 0, -1));
    EXPECT_FALSE(geometry.edge_push(50, 20, 1, 0));
}

TEST(CursorGeometryTest, IgnoresEmptySurfaceOrBitmap)
{
    const kernel::display::CursorGeometry empty_surface({0, 0, 0, 50}, 10, 16);
    const kernel::display::CursorGeometry empty_bitmap({0, 0, 100, 50}, 0, 16);

    EXPECT_TRUE(empty_surface.visible_rect(0, 0).empty());
    EXPECT_TRUE(empty_bitmap.visible_rect(0, 0).empty());
}
