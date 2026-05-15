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
    EXPECT_FALSE(request.repaint_text_layer);
    EXPECT_FALSE(request.repaint_higher_layers);
    EXPECT_TRUE(request.dirty_rect.empty());
}

} // namespace

TEST(TerminalRepaintStateTest, NonBatchScrollRequestsImmediatePixelScroll)
{
    kernel::display::TerminalRepaintState state;

    const kernel::display::TerminalRepaintRequest request = state.record_scroll({0, 0, 640, 480}, 24);

    EXPECT_TRUE(request.repaint_text_layer);
    EXPECT_FALSE(request.full_text_repaint);
    EXPECT_TRUE(request.repaint_higher_layers);
    EXPECT_EQ(request.scroll_rows, 1u);
    expect_rect(request.dirty_rect, 0, 0, 640, 480);
    EXPECT_FALSE(state.pending_text_repaint());
}

TEST(TerminalRepaintStateTest, BatchScrollDefersPixelScroll)
{
    kernel::display::TerminalRepaintState state;
    EXPECT_TRUE(state.begin_batch());

    const kernel::display::TerminalRepaintRequest request = state.record_scroll({0, 0, 640, 480}, 24);

    EXPECT_FALSE(request.repaint_text_layer);
    EXPECT_FALSE(request.repaint_higher_layers);
    EXPECT_TRUE(state.pending_text_repaint());
    EXPECT_FALSE(state.pending_full_text_repaint());
    EXPECT_EQ(state.pending_scroll_rows(), 1u);

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    EXPECT_TRUE(flush.outermost_batch_ended);
    EXPECT_TRUE(flush.repaint_text_layer);
    EXPECT_FALSE(flush.full_text_repaint);
    EXPECT_TRUE(flush.repaint_higher_layers);
    EXPECT_EQ(flush.scroll_rows, 1u);
    expect_rect(flush.dirty_rect, 0, 0, 640, 480);
}

TEST(TerminalRepaintStateTest, MultipleBatchScrollsCollapseToOnePixelScrollPlan)
{
    kernel::display::TerminalRepaintState state;
    EXPECT_TRUE(state.begin_batch());

    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 24));
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 24));
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 24));

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    EXPECT_TRUE(flush.repaint_text_layer);
    EXPECT_FALSE(flush.full_text_repaint);
    EXPECT_TRUE(flush.repaint_higher_layers);
    EXPECT_EQ(flush.scroll_rows, 3u);
    expect_rect(flush.dirty_rect, 0, 0, 640, 480);
}

TEST(TerminalRepaintStateTest, PendingScrollDominatesSmallerDirtyRects)
{
    kernel::display::TerminalRepaintState state;
    EXPECT_TRUE(state.begin_batch());

    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 24));
    expect_no_immediate_repaint(state.record_dirty({10, 10, 8, 8}));

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    EXPECT_TRUE(flush.repaint_text_layer);
    EXPECT_FALSE(flush.full_text_repaint);
    expect_rect(flush.dirty_rect, 0, 0, 640, 480);
}

TEST(TerminalRepaintStateTest, TooManyBatchScrollsRequestFullRepaintFallback)
{
    kernel::display::TerminalRepaintState state;
    EXPECT_TRUE(state.begin_batch());

    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 2));
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 2));

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    EXPECT_TRUE(flush.repaint_text_layer);
    EXPECT_TRUE(flush.full_text_repaint);
    EXPECT_EQ(flush.scroll_rows, 0u);
    expect_rect(flush.dirty_rect, 0, 0, 640, 480);
}

TEST(TerminalRepaintStateTest, BatchEndClearsPendingState)
{
    kernel::display::TerminalRepaintState state;
    EXPECT_TRUE(state.begin_batch());
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}, 24));

    const kernel::display::TerminalRepaintFlush first_flush = state.end_batch();
    EXPECT_TRUE(first_flush.outermost_batch_ended);
    EXPECT_FALSE(state.pending_text_repaint());

    EXPECT_TRUE(state.begin_batch());
    const kernel::display::TerminalRepaintFlush second_flush = state.end_batch();
    EXPECT_TRUE(second_flush.outermost_batch_ended);
    EXPECT_FALSE(second_flush.repaint_text_layer);
    EXPECT_FALSE(second_flush.full_text_repaint);
    EXPECT_FALSE(second_flush.repaint_higher_layers);
    EXPECT_EQ(second_flush.scroll_rows, 0u);
    EXPECT_TRUE(second_flush.dirty_rect.empty());
}
