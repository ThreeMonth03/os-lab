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

void expect_no_immediate_repaint(kernel::display::TerminalRepaintRequest request)
{
    EXPECT_TRUE(request.damage.empty());
}

} // namespace

TEST(TerminalRepaintStateTest, NonBatchScrollRequestsScrollDamage)
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
    EXPECT_FALSE(state.pending_damage());
}

TEST(TerminalRepaintStateTest, BatchScrollDefersScrollDamage)
{
    kernel::display::TerminalRepaintState state;
    state.reset({0, 0, 640, 480});
    state.begin_batch();
    EXPECT_TRUE(state.in_batch());

    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 18));
    EXPECT_TRUE(state.pending_damage());

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    EXPECT_TRUE(flush.outermost_batch_ended);
    ASSERT_TRUE(flush.damage.has_scroll());
    expect_rect(flush.damage.scroll.rect, 0, 0, 640, 480);
    EXPECT_EQ(flush.damage.scroll.distance, 18u);
    ASSERT_TRUE(flush.damage.has_dirty());
    expect_rect(flush.damage.dirty_rect, 0, 462, 640, 18);
}

TEST(TerminalRepaintStateTest, MultipleBatchScrollsCollapseToOneScrollDamage)
{
    kernel::display::TerminalRepaintState state;
    state.reset({0, 0, 640, 480});
    state.begin_batch();

    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 18));
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 18));
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 18));

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    ASSERT_TRUE(flush.damage.has_scroll());
    expect_rect(flush.damage.scroll.rect, 0, 0, 640, 480);
    EXPECT_EQ(flush.damage.scroll.distance, 54u);
    ASSERT_TRUE(flush.damage.has_dirty());
    expect_rect(flush.damage.dirty_rect, 0, 426, 640, 54);
}

TEST(TerminalRepaintStateTest, DirtyAndScrollDamageCanShareOneFlush)
{
    kernel::display::TerminalRepaintState state;
    state.reset({0, 0, 640, 480});
    state.begin_batch();

    expect_no_immediate_repaint(state.record_dirty({10, 10, 8, 8}));
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 18));

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    ASSERT_TRUE(flush.damage.has_scroll());
    ASSERT_TRUE(flush.damage.has_dirty());
    expect_rect(flush.damage.dirty_rect, 0, 10, 640, 470);
}

TEST(TerminalRepaintStateTest, BatchEndClearsPendingState)
{
    kernel::display::TerminalRepaintState state;
    state.reset({0, 0, 640, 480});
    state.begin_batch();
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 18));

    const kernel::display::TerminalRepaintFlush first_flush = state.end_batch();
    EXPECT_TRUE(first_flush.outermost_batch_ended);
    EXPECT_FALSE(state.pending_damage());

    state.begin_batch();
    const kernel::display::TerminalRepaintFlush second_flush = state.end_batch();
    EXPECT_TRUE(second_flush.outermost_batch_ended);
    EXPECT_TRUE(second_flush.damage.empty());
}
