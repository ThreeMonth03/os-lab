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

TEST(FrameDamageTest, PreservesDirtyBeforeScrollOrder)
{
    kernel::display::DamageAccumulator damage({0, 0, 80, 100});

    damage.mark_dirty({0, 72, 80, 18});
    damage.record_scroll({0, 0, 80, 90}, 18);

    const kernel::display::FrameDamage flush = damage.flush();
    ASSERT_EQ(flush.step_count, 3u);
    EXPECT_EQ(flush.steps[0].kind, kernel::display::FrameDamageStepKind::DirtyRect);
    expect_rect(flush.steps[0].rect, 0, 72, 80, 18);
    EXPECT_EQ(flush.steps[1].kind, kernel::display::FrameDamageStepKind::Scroll);
    expect_rect(flush.steps[1].rect, 0, 0, 80, 90);
    EXPECT_EQ(flush.steps[1].distance, 18u);
    EXPECT_EQ(flush.steps[2].kind, kernel::display::FrameDamageStepKind::DirtyRect);
    expect_rect(flush.steps[2].rect, 0, 72, 80, 18);
}

TEST(FrameDamageTest, DifferentScrollRegionsPreserveOrderedSteps)
{
    kernel::display::DamageAccumulator damage({0, 0, 80, 100});

    damage.record_scroll({0, 0, 80, 90}, 18);
    damage.record_scroll({0, 10, 80, 80}, 18);

    const kernel::display::FrameDamage flush = damage.flush();
    ASSERT_EQ(flush.step_count, 4u);
    EXPECT_EQ(flush.steps[0].kind, kernel::display::FrameDamageStepKind::Scroll);
    expect_rect(flush.steps[0].rect, 0, 0, 80, 90);
    EXPECT_EQ(flush.steps[1].kind, kernel::display::FrameDamageStepKind::DirtyRect);
    expect_rect(flush.steps[1].rect, 0, 72, 80, 18);
    EXPECT_EQ(flush.steps[2].kind, kernel::display::FrameDamageStepKind::Scroll);
    expect_rect(flush.steps[2].rect, 0, 10, 80, 80);
    EXPECT_EQ(flush.steps[3].kind, kernel::display::FrameDamageStepKind::DirtyRect);
    expect_rect(flush.steps[3].rect, 0, 72, 80, 18);
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

TEST(FrameDamageTest, ScrollDirtyRegionUpTracksFinalDirtyPosition)
{
    const kernel::display::Rect dirty =
        kernel::display::scroll_dirty_region_up({0, 72, 80, 18}, {0, 0, 80, 90}, 18);

    expect_rect(dirty, 0, 54, 80, 18);
}

TEST(FrameDamageTest, ScrollDirtyRegionUpDropsContentScrolledOut)
{
    const kernel::display::Rect dirty =
        kernel::display::scroll_dirty_region_up({0, 0, 80, 18}, {0, 0, 80, 90}, 18);

    EXPECT_TRUE(dirty.empty());
}

TEST(FrameDamageTest, ScrollDirtyRegionUpClipsPartialDirtyRegion)
{
    const kernel::display::Rect dirty =
        kernel::display::scroll_dirty_region_up({0, 9, 80, 18}, {0, 0, 80, 90}, 18);

    expect_rect(dirty, 0, 0, 80, 9);
}

TEST(FrameDamageTest, LayerAccumulatorCollapsesMultipleScrollsToFinalDirtyRegion)
{
    kernel::display::LayerDamageAccumulator accumulator({0, 0, 100, 100});

    kernel::display::FrameDamage first;
    ASSERT_TRUE(first.append_dirty({0, 70, 80, 10}));
    ASSERT_TRUE(first.append_scroll({{0, 10, 80, 80}, 10}));
    ASSERT_TRUE(first.append_dirty({0, 80, 80, 10}, true));
    accumulator.record(first);

    kernel::display::FrameDamage second;
    ASSERT_TRUE(second.append_dirty({8, 70, 12, 10}));
    ASSERT_TRUE(second.append_scroll({{0, 10, 80, 80}, 10}));
    ASSERT_TRUE(second.append_dirty({0, 80, 80, 10}, true));
    accumulator.record(second);

    const kernel::display::FrameDamage flush = accumulator.flush();

    EXPECT_FALSE(flush.has_scroll());
    ASSERT_TRUE(flush.has_dirty());
    expect_rect(flush.dirty_rect, 0, 10, 80, 80);
    ASSERT_EQ(flush.step_count, 1u);
    EXPECT_EQ(flush.steps[0].kind, kernel::display::FrameDamageStepKind::DirtyRect);
}

TEST(FrameDamageTest, LayerAccumulatorPreservesSingleScrollOrdering)
{
    kernel::display::LayerDamageAccumulator accumulator({0, 0, 100, 100});

    kernel::display::FrameDamage damage;
    ASSERT_TRUE(damage.append_dirty({0, 70, 80, 10}));
    ASSERT_TRUE(damage.append_scroll({{0, 10, 80, 80}, 10}));
    ASSERT_TRUE(damage.append_dirty({0, 80, 80, 10}, true));
    accumulator.record(damage);

    const kernel::display::FrameDamage flush = accumulator.flush();

    ASSERT_TRUE(flush.has_scroll());
    ASSERT_EQ(flush.step_count, 3u);
    EXPECT_EQ(flush.steps[0].kind, kernel::display::FrameDamageStepKind::DirtyRect);
    EXPECT_EQ(flush.steps[1].kind, kernel::display::FrameDamageStepKind::Scroll);
    EXPECT_EQ(flush.steps[2].kind, kernel::display::FrameDamageStepKind::DirtyRect);
    EXPECT_TRUE(accumulator.empty());
}
