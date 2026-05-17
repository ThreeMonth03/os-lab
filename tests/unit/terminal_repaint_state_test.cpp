#include <gtest/gtest.h>

#include "kernel/display/terminal_repaint_state.hpp"

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

TEST(TerminalRepaintStateTest, DirtyRequestsImmediateDamage)
{
    kernel::display::TerminalRepaintState state;
    state.reset({0, 0, 640, 480});

    const kernel::display::TerminalRepaintRequest request = state.record_dirty({10, 20, 8, 8});

    ASSERT_TRUE(request.damage.has_dirty());
    expect_rect(request.damage.dirty_rect, 10, 20, 8, 8);
    ASSERT_EQ(request.damage.step_count, 1u);
    EXPECT_EQ(request.damage.steps[0].kind, kernel::display::FrameDamageStepKind::DirtyRect);
}

TEST(TerminalRepaintStateTest, ScrollRequestsOrderedImmediateDamage)
{
    kernel::display::TerminalRepaintState state;
    state.reset({0, 0, 640, 480});

    const kernel::display::TerminalRepaintRequest request =
        state.record_scroll({0, 0, 640, 480}, 18);

    ASSERT_TRUE(request.damage.has_scroll());
    expect_rect(request.damage.scroll.rect, 0, 0, 640, 480);
    EXPECT_EQ(request.damage.scroll.distance, 18u);
    ASSERT_TRUE(request.damage.has_dirty());
    expect_rect(request.damage.dirty_rect, 0, 462, 640, 18);
    ASSERT_EQ(request.damage.step_count, 2u);
    EXPECT_EQ(request.damage.steps[0].kind, kernel::display::FrameDamageStepKind::Scroll);
    EXPECT_EQ(request.damage.steps[1].kind, kernel::display::FrameDamageStepKind::DirtyRect);
}

TEST(TerminalRepaintStateTest, ClipsDamageToTerminalBounds)
{
    kernel::display::TerminalRepaintState state;
    state.reset({0, 0, 640, 480});

    const kernel::display::TerminalRepaintRequest request = state.record_dirty({630, 470, 20, 20});

    ASSERT_TRUE(request.damage.has_dirty());
    expect_rect(request.damage.dirty_rect, 630, 470, 10, 10);
}
