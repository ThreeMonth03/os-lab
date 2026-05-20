#include <gtest/gtest.h>

#include "kernel/display/window_repaint_planner.hpp"

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

bool contains(kernel::display::Rect rect, uint64_t x, uint64_t y)
{
    return !rect.empty() && x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

bool contained_by_any(const kernel::display::WindowRepaintRegionList & regions,
                      uint64_t x,
                      uint64_t y)
{
    for (size_t index = 0; index < regions.count(); ++index)
    {
        if (contains(regions.at(index), x, y))
        {
            return true;
        }
    }
    return false;
}

kernel::display::WindowSessionBounds bounds_for(kernel::display::Rect outer)
{
    return {
        outer,
        {outer.x + 4, outer.y + 20, outer.width - 8, outer.height - 24},
    };
}

kernel::display::WindowSession terminal_session(bool focused = true,
                                                bool active = true,
                                                kernel::display::Rect outer = {10, 20, 320, 200})
{
    return kernel::display::make_terminal_window_session(kernel::display::kTerminalWindowSessionId,
                                                         kernel::display::kTerminalAppSurfaceId,
                                                         bounds_for(outer),
                                                         true,
                                                         true,
                                                         focused,
                                                         active);
}

kernel::display::WindowSession dummy_session(bool focused = false,
                                             bool active = false,
                                             kernel::display::Rect outer = {120, 80, 240, 160})
{
    return kernel::display::make_app_window_session(kernel::display::kDummyDebugWindowSessionId,
                                                    kernel::display::kDummyDebugAppSurfaceId,
                                                    bounds_for(outer),
                                                    kernel::display::WindowSessionRole::DummyDebugApp,
                                                    true,
                                                    true,
                                                    focused,
                                                    active);
}

kernel::display::WindowRepaintPlanner planner()
{
    return {{0, 0, 800, 600}, kernel::display::terminal_window_frame_config(true)};
}

} // namespace

TEST(WindowRepaintPlannerTest, MoveDamageKeepsOldAndNewRegionsSeparate)
{
    const kernel::display::WindowRepaintRegionList regions =
        planner().move_damage({10, 20, 320, 200}, {40, 50, 320, 200});

    ASSERT_EQ(regions.count(), 3u);
    expect_rect(regions.at(0), 40, 50, 320, 200);
    expect_rect(regions.at(1), 10, 20, 320, 30);
    expect_rect(regions.at(2), 10, 50, 30, 170);
    EXPECT_LT(regions.total_area(), 800u * 600u);
    EXPECT_FALSE(regions.full_screen_fallback());
}

TEST(WindowRepaintPlannerTest, MoveExposedDamageExcludesNewBoundsForSceneCopyFastPath)
{
    const kernel::display::WindowRepaintRegionList regions =
        planner().move_exposed_damage({10, 20, 320, 200}, {40, 50, 320, 200});

    ASSERT_EQ(regions.count(), 2u);
    expect_rect(regions.at(0), 10, 20, 320, 30);
    expect_rect(regions.at(1), 10, 50, 30, 170);
    EXPECT_LT(regions.total_area(), 320u * 200u);
    EXPECT_FALSE(regions.full_screen_fallback());
}

TEST(WindowRepaintPlannerTest, RepeatedMoveStepsDoNotFallbackToFullscreen)
{
    kernel::display::Rect previous{10, 20, 320, 200};
    for (uint64_t step = 0; step < 12; ++step)
    {
        const kernel::display::Rect current{10 + ((step + 1) * 8), 20 + ((step + 1) * 4), 320, 200};
        const kernel::display::WindowRepaintRegionList regions =
            planner().move_damage(previous, current);
        EXPECT_FALSE(regions.full_screen_fallback());
        EXPECT_GE(regions.count(), 2u);
        EXPECT_LT(regions.largest_area(), 800u * 600u);
        EXPECT_LE(regions.total_area(), (320u * 200u) + (320u * 4u) + (8u * 196u));
        previous = current;
    }
}

TEST(WindowRepaintPlannerTest, VisualStateDamageIsLimitedToChrome)
{
    const kernel::display::WindowRepaintRegionList regions =
        planner().visual_state_damage({10, 20, 320, 200});

    ASSERT_EQ(regions.count(), 4u);
    expect_rect(regions.at(0), 10, 20, 320, 20);
    expect_rect(regions.at(1), 10, 40, 1, 180);
    expect_rect(regions.at(2), 329, 40, 1, 180);
    expect_rect(regions.at(3), 10, 219, 320, 1);
    EXPECT_LT(regions.total_area(), 320u * 200u);
}

TEST(WindowRepaintPlannerTest, VisualStateDamageCoversPaintedChromeOutline)
{
    const kernel::display::Rect bounds{10, 20, 320, 200};
    const kernel::display::WindowFrameMetrics metrics =
        kernel::display::WindowChrome::metrics_for(
            bounds,
            kernel::display::terminal_window_frame_config(true));
    const kernel::display::WindowRepaintRegionList regions =
        planner().visual_state_damage(bounds);

    for (uint64_t y = bounds.y; y < bounds.y + bounds.height; ++y)
    {
        for (uint64_t x = bounds.x; x < bounds.x + bounds.width; ++x)
        {
            if (kernel::display::WindowChrome::outline_contains_pixel(metrics, x, y))
            {
                EXPECT_TRUE(contained_by_any(regions, x, y))
                    << "painted chrome pixel outside visual damage at " << x << "," << y;
            }
        }
    }
}

TEST(WindowRepaintPlannerTest, VisualStateTransitionDamagesOldAndNewActiveChrome)
{
    kernel::display::WindowSessionRegistry sessions;
    ASSERT_TRUE(sessions.register_session(terminal_session(false, false)));
    ASSERT_TRUE(sessions.register_session(dummy_session(true, true)));

    kernel::display::WindowStack previous;
    ASSERT_TRUE(previous.register_window(terminal_session(false, false)));
    ASSERT_TRUE(previous.register_window(dummy_session(true, true)));

    kernel::display::WindowStack current = previous;
    ASSERT_TRUE(current.focus_and_activate(kernel::display::kTerminalWindowSessionId));
    ASSERT_TRUE(current.raise_to_front(kernel::display::kTerminalWindowSessionId));

    ASSERT_TRUE(sessions.restore_session(terminal_session(true, true)));
    ASSERT_TRUE(sessions.restore_session(dummy_session(false, false)));

    const kernel::display::WindowRepaintRegionList regions =
        planner().visual_state_transition_damage(previous, current, sessions);

    EXPECT_TRUE(contained_by_any(regions, 10, 20))
        << "terminal active chrome was not damaged";
    EXPECT_TRUE(contained_by_any(regions, 120, 80))
        << "dummy inactive chrome was not damaged";
    EXPECT_LT(regions.total_area(), 800u * 600u);
    EXPECT_FALSE(regions.full_screen_fallback());
}

TEST(WindowRepaintPlannerTest, RaiseDamageOnlyIncludesPreviouslyObscuredOverlapAndChrome)
{
    kernel::display::WindowSessionRegistry sessions;
    const kernel::display::WindowSession terminal = terminal_session(true, true);
    const kernel::display::WindowSession dummy = dummy_session(false, false);
    ASSERT_TRUE(sessions.register_session(terminal));
    ASSERT_TRUE(sessions.register_session(dummy));

    kernel::display::WindowStack previous;
    ASSERT_TRUE(previous.register_window(terminal));
    ASSERT_TRUE(previous.register_window(dummy));

    kernel::display::WindowStack current = previous;
    ASSERT_TRUE(current.focus_and_activate(kernel::display::kTerminalWindowSessionId));
    ASSERT_TRUE(current.raise_to_front(kernel::display::kTerminalWindowSessionId));

    ASSERT_TRUE(sessions.restore_session(terminal_session(true, true)));
    ASSERT_TRUE(sessions.restore_session(dummy_session(false, false)));

    const kernel::display::WindowRepaintRegionList regions =
        planner().stack_transition_damage(previous, current, sessions);

    ASSERT_GE(regions.count(), 1u);
    expect_rect(regions.at(0), 120, 80, 210, 140);
    EXPECT_LT(regions.total_area(), 800u * 600u);
    EXPECT_FALSE(regions.full_screen_fallback());
}
