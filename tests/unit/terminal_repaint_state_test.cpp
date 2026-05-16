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

TEST(TerminalRepaintStateTest, NonBatchScrollRequestsTextLayerRepaint)
{
    kernel::display::TerminalRepaintState state;

    const kernel::display::TerminalRepaintRequest request = state.record_scroll({0, 0, 640, 480});

    EXPECT_TRUE(request.repaint_text_layer);
    EXPECT_TRUE(request.repaint_entire_text_layer);
    EXPECT_TRUE(request.repaint_higher_layers);
    expect_rect(request.dirty_rect, 0, 0, 640, 480);
    EXPECT_FALSE(state.pending_text_repaint());
}

TEST(TerminalRepaintStateTest, BatchScrollDefersTextLayerRepaint)
{
    kernel::display::TerminalRepaintState state;
    state.begin_batch();
    EXPECT_TRUE(state.in_batch());

    const kernel::display::TerminalRepaintRequest request = state.record_scroll({0, 0, 640, 480});

    EXPECT_FALSE(request.repaint_text_layer);
    EXPECT_FALSE(request.repaint_higher_layers);
    EXPECT_TRUE(state.pending_text_repaint());

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    EXPECT_TRUE(flush.outermost_batch_ended);
    EXPECT_TRUE(flush.repaint_text_layer);
    EXPECT_TRUE(flush.repaint_entire_text_layer);
    EXPECT_TRUE(flush.repaint_higher_layers);
    expect_rect(flush.dirty_rect, 0, 0, 640, 480);
}

TEST(TerminalRepaintStateTest, MultipleBatchScrollsCollapseToOneTextLayerRepaint)
{
    kernel::display::TerminalRepaintState state;
    state.begin_batch();
    EXPECT_TRUE(state.in_batch());

    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}));
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}));
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}));

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    EXPECT_TRUE(flush.repaint_text_layer);
    EXPECT_TRUE(flush.repaint_entire_text_layer);
    EXPECT_TRUE(flush.repaint_higher_layers);
    expect_rect(flush.dirty_rect, 0, 0, 640, 480);
}

TEST(TerminalRepaintStateTest, PendingScrollDominatesSmallerDirtyRects)
{
    kernel::display::TerminalRepaintState state;
    state.begin_batch();
    EXPECT_TRUE(state.in_batch());

    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}));
    expect_no_immediate_repaint(state.record_dirty({10, 10, 8, 8}));

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    EXPECT_TRUE(flush.repaint_text_layer);
    EXPECT_TRUE(flush.repaint_entire_text_layer);
    expect_rect(flush.dirty_rect, 0, 0, 640, 480);
}

TEST(TerminalRepaintStateTest, ScrollAlwaysRequestsEntireTextLayerRepaint)
{
    kernel::display::TerminalRepaintState state;
    state.begin_batch();
    EXPECT_TRUE(state.in_batch());

    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}));
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}));

    const kernel::display::TerminalRepaintFlush flush = state.end_batch();
    EXPECT_TRUE(flush.repaint_text_layer);
    EXPECT_TRUE(flush.repaint_entire_text_layer);
    expect_rect(flush.dirty_rect, 0, 0, 640, 480);
}

TEST(TerminalRepaintStateTest, BatchEndClearsPendingState)
{
    kernel::display::TerminalRepaintState state;
    state.begin_batch();
    EXPECT_TRUE(state.in_batch());
    expect_no_immediate_repaint(state.record_scroll({0, 0, 640, 480}));

    const kernel::display::TerminalRepaintFlush first_flush = state.end_batch();
    EXPECT_TRUE(first_flush.outermost_batch_ended);
    EXPECT_FALSE(state.pending_text_repaint());

    state.begin_batch();
    EXPECT_TRUE(state.in_batch());
    const kernel::display::TerminalRepaintFlush second_flush = state.end_batch();
    EXPECT_TRUE(second_flush.outermost_batch_ended);
    EXPECT_FALSE(second_flush.repaint_text_layer);
    EXPECT_FALSE(second_flush.repaint_entire_text_layer);
    EXPECT_FALSE(second_flush.repaint_higher_layers);
    EXPECT_TRUE(second_flush.dirty_rect.empty());
}
