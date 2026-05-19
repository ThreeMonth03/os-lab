#include <gtest/gtest.h>

#include "kernel/display/window_stack.hpp"

namespace
{

kernel::display::WindowSessionBounds bounds_for(uint64_t x = 0)
{
    return {
        {x, 10, 320, 200},
        {x + 4, 26, 312, 180},
    };
}

kernel::display::WindowSession make_window(kernel::display::WindowSessionId id,
                                           bool visible = true,
                                           bool focused = false,
                                           bool active = false)
{
    return kernel::display::make_terminal_window_session(
        id,
        static_cast<kernel::display::AppSurfaceId>(id),
        bounds_for(static_cast<uint64_t>(id) * 10),
        true,
        visible,
        focused,
        active);
}

} // namespace

TEST(WindowStackTest, RegistersAndFindsWindow)
{
    kernel::display::WindowStack stack;
    ASSERT_TRUE(stack.register_window(make_window(kernel::display::kTerminalWindowSessionId,
                                                  true,
                                                  true,
                                                  true)));

    const kernel::display::WindowStackEntry * entry =
        stack.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(entry, nullptr);
    EXPECT_TRUE(entry->open());
    EXPECT_TRUE(entry->focused);
    EXPECT_TRUE(entry->active);
    EXPECT_EQ(stack.focused_window(), kernel::display::kTerminalWindowSessionId);
    EXPECT_EQ(stack.active_window(), kernel::display::kTerminalWindowSessionId);
}

TEST(WindowStackTest, RejectsDuplicateAndFullRegistrations)
{
    kernel::display::WindowStack stack;
    ASSERT_TRUE(stack.register_window(make_window(1)));
    EXPECT_FALSE(stack.register_window(make_window(1)));
    ASSERT_TRUE(stack.register_window(make_window(2)));
    ASSERT_TRUE(stack.register_window(make_window(3)));
    ASSERT_TRUE(stack.register_window(make_window(4)));

    EXPECT_FALSE(stack.register_window(make_window(5)));
}

TEST(WindowStackTest, FocusVisibleWindowAndRejectHiddenOrClosed)
{
    kernel::display::WindowStack stack;
    ASSERT_TRUE(stack.register_window(make_window(1, true)));
    ASSERT_TRUE(stack.register_window(make_window(2, false)));

    EXPECT_TRUE(stack.focus_window(1));
    EXPECT_EQ(stack.focused_window(), 1);
    EXPECT_FALSE(stack.focus_window(2));

    ASSERT_TRUE(stack.set_visible(2, true));
    ASSERT_TRUE(stack.close_window(2));
    EXPECT_FALSE(stack.focus_window(2));
}

TEST(WindowStackTest, ActivateVisibleWindowAndRejectHiddenOrClosed)
{
    kernel::display::WindowStack stack;
    ASSERT_TRUE(stack.register_window(make_window(1, true)));
    ASSERT_TRUE(stack.register_window(make_window(2, false)));

    EXPECT_TRUE(stack.activate_window(1));
    EXPECT_EQ(stack.active_window(), 1);
    EXPECT_FALSE(stack.activate_window(2));

    ASSERT_TRUE(stack.set_visible(2, true));
    ASSERT_TRUE(stack.close_window(2));
    EXPECT_FALSE(stack.activate_window(2));
}

TEST(WindowStackTest, RaiseToFrontPreservesOrder)
{
    kernel::display::WindowStack stack;
    ASSERT_TRUE(stack.register_window(make_window(1)));
    ASSERT_TRUE(stack.register_window(make_window(2)));
    ASSERT_TRUE(stack.register_window(make_window(3)));

    ASSERT_TRUE(stack.raise_to_front(1));

    ASSERT_NE(stack.at(0), nullptr);
    ASSERT_NE(stack.at(1), nullptr);
    ASSERT_NE(stack.at(2), nullptr);
    EXPECT_EQ(stack.at(0)->id, 2);
    EXPECT_EQ(stack.at(1)->id, 3);
    EXPECT_EQ(stack.at(2)->id, 1);
    EXPECT_EQ(stack.topmost_visible_window(), 1);
}

TEST(WindowStackTest, HideAndCloseClearFocusAndActive)
{
    kernel::display::WindowStack stack;
    ASSERT_TRUE(stack.register_window(make_window(1, true, true, true)));

    ASSERT_TRUE(stack.set_visible(1, false));
    EXPECT_EQ(stack.focused_window(), kernel::display::kInvalidWindowSessionId);
    EXPECT_EQ(stack.active_window(), kernel::display::kInvalidWindowSessionId);

    ASSERT_TRUE(stack.set_visible(1, true));
    ASSERT_TRUE(stack.focus_and_activate(1));
    ASSERT_TRUE(stack.close_window(1));
    EXPECT_EQ(stack.focused_window(), kernel::display::kInvalidWindowSessionId);
    EXPECT_EQ(stack.active_window(), kernel::display::kInvalidWindowSessionId);
    EXPECT_EQ(stack.topmost_visible_window(), kernel::display::kInvalidWindowSessionId);
}

TEST(WindowStackTest, TopmostVisibleSkipsHiddenAndClosedWindows)
{
    kernel::display::WindowStack stack;
    ASSERT_TRUE(stack.register_window(make_window(1, true)));
    ASSERT_TRUE(stack.register_window(make_window(2, true)));
    ASSERT_TRUE(stack.register_window(make_window(3, true)));

    ASSERT_TRUE(stack.set_visible(3, false));
    EXPECT_EQ(stack.topmost_visible_window(), 2);

    ASSERT_TRUE(stack.close_window(2));
    EXPECT_EQ(stack.topmost_visible_window(), 1);
}

TEST(WindowStackTest, SyncWindowUpdatesPolicyState)
{
    kernel::display::WindowStack stack;
    ASSERT_TRUE(stack.register_window(make_window(1, true, true, true)));

    kernel::display::WindowSession hidden = make_window(1, false, false, false);
    ASSERT_TRUE(stack.sync_window(hidden));

    const kernel::display::WindowStackEntry * entry = stack.find(1);
    ASSERT_NE(entry, nullptr);
    EXPECT_TRUE(entry->hidden());
    EXPECT_FALSE(entry->focused);
    EXPECT_FALSE(entry->active);
}
