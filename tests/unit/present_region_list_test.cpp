#include <gtest/gtest.h>

#include "kernel/display/present_region_list.hpp"

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

TEST(PresentRegionListTest, AppendsAndClipsRects)
{
    kernel::display::PresentRegionList regions({0, 0, 100, 50});

    EXPECT_EQ(regions.append({90, 40, 20, 20}),
              kernel::display::PresentRegionAppendResult::Queued);

    ASSERT_EQ(regions.count(), 1u);
    expect_rect(regions.at(0), 90, 40, 10, 10);
}

TEST(PresentRegionListTest, KeepsDisjointRectsSeparate)
{
    kernel::display::PresentRegionList regions({0, 0, 100, 50});

    EXPECT_EQ(regions.append({1, 1, 2, 2}),
              kernel::display::PresentRegionAppendResult::Queued);
    EXPECT_EQ(regions.append({80, 40, 4, 4}),
              kernel::display::PresentRegionAppendResult::Queued);

    EXPECT_EQ(regions.count(), 2u);
    expect_rect(regions.at(0), 1, 1, 2, 2);
    expect_rect(regions.at(1), 80, 40, 4, 4);
}

TEST(PresentRegionListTest, MergesOverlappingRects)
{
    kernel::display::PresentRegionList regions({0, 0, 100, 50});

    EXPECT_EQ(regions.append({10, 10, 8, 8}),
              kernel::display::PresentRegionAppendResult::Queued);
    EXPECT_EQ(regions.append({14, 14, 8, 8}),
              kernel::display::PresentRegionAppendResult::Merged);

    ASSERT_EQ(regions.count(), 1u);
    expect_rect(regions.at(0), 10, 10, 12, 12);
}

TEST(PresentRegionListTest, MergesAlignedAdjacentRects)
{
    kernel::display::PresentRegionList regions({0, 0, 100, 50});

    EXPECT_EQ(regions.append({10, 10, 8, 8}),
              kernel::display::PresentRegionAppendResult::Queued);
    EXPECT_EQ(regions.append({18, 10, 8, 8}),
              kernel::display::PresentRegionAppendResult::Merged);

    ASSERT_EQ(regions.count(), 1u);
    expect_rect(regions.at(0), 10, 10, 16, 8);
}

TEST(PresentRegionListTest, CapacityFallsBackToFullBounds)
{
    kernel::display::PresentRegionList regions({0, 0, 100, 50});

    for (size_t index = 0; index < regions.capacity(); ++index)
    {
        EXPECT_EQ(regions.append({index * 3, 0, 1, 1}),
                  kernel::display::PresentRegionAppendResult::Queued);
    }

    EXPECT_EQ(regions.append({99, 49, 1, 1}),
              kernel::display::PresentRegionAppendResult::FullscreenFallback);

    ASSERT_EQ(regions.count(), 1u);
    EXPECT_TRUE(regions.full_screen_fallback());
    expect_rect(regions.at(0), 0, 0, 100, 50);
}
