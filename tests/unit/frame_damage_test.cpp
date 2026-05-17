#include <gtest/gtest.h>

#include "kernel/display/frame_damage.hpp"

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

TEST(FrameDamageTest, MultipleScrollsCollapseIntoOneAccumulatedScroll)
{
    kernel::display::DamageAccumulator damage({0, 0, 80, 100});

    damage.record_scroll({0, 0, 80, 90}, 18);
    damage.record_scroll({0, 0, 80, 90}, 18);
    damage.record_scroll({0, 0, 80, 90}, 18);

    const kernel::display::FrameDamage flush = damage.flush();
    ASSERT_TRUE(flush.has_scroll());
    expect_rect(flush.scroll.rect, 0, 0, 80, 90);
    EXPECT_EQ(flush.scroll.distance, 54u);
    ASSERT_TRUE(flush.has_dirty());
    expect_rect(flush.dirty_rect, 0, 36, 80, 54);
    EXPECT_TRUE(damage.empty());
}

TEST(FrameDamageTest, RectDirtyAndScrollDamageCanCoexist)
{
    kernel::display::DamageAccumulator damage({0, 0, 80, 100});

    damage.mark_dirty({2, 2, 4, 4});
    damage.record_scroll({0, 0, 80, 90}, 18);

    const kernel::display::FrameDamage flush = damage.flush();
    ASSERT_TRUE(flush.has_scroll());
    ASSERT_TRUE(flush.has_dirty());
    expect_rect(flush.dirty_rect, 0, 2, 80, 88);
}

TEST(FrameDamageTest, DifferentScrollRegionsFallbackToDirtyRects)
{
    kernel::display::DamageAccumulator damage({0, 0, 80, 100});

    damage.record_scroll({0, 0, 80, 90}, 18);
    damage.record_scroll({0, 10, 80, 80}, 18);

    const kernel::display::FrameDamage flush = damage.flush();
    EXPECT_FALSE(flush.has_scroll());
    ASSERT_TRUE(flush.has_dirty());
    expect_rect(flush.dirty_rect, 0, 0, 80, 90);
}

TEST(FrameDamageTest, ScrollOverflowFallbacksToDirtyRegion)
{
    kernel::display::DamageAccumulator damage({0, 0, 80, 100});

    damage.record_scroll({0, 0, 80, 36}, 18);
    damage.record_scroll({0, 0, 80, 36}, 18);

    const kernel::display::FrameDamage flush = damage.flush();
    EXPECT_FALSE(flush.has_scroll());
    ASSERT_TRUE(flush.has_dirty());
    expect_rect(flush.dirty_rect, 0, 0, 80, 36);
}

TEST(FrameDamageTest, ExposedScrollRegionIsBottomSlice)
{
    const kernel::display::Rect exposed =
        kernel::display::exposed_scroll_region({{4, 8, 20, 40}, 12});

    expect_rect(exposed, 4, 36, 20, 12);
}
