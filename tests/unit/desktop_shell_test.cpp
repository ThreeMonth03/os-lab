#include <gtest/gtest.h>

#include "kernel/display/desktop_shell.hpp"

namespace
{

kernel::display::WindowSession terminal_session(bool visible, bool focused, bool closed)
{
    kernel::display::WindowSession session =
        kernel::display::make_terminal_window_session(kernel::display::kTerminalWindowSessionId,
                                                      kernel::display::kTerminalAppSurfaceId,
                                                      {{0, 0, 320, 200}, {4, 16, 312, 180}},
                                                      true,
                                                      visible,
                                                      focused,
                                                      focused);
    if (closed)
    {
        session.state = kernel::display::WindowSessionState::Closed;
        session.focused = false;
        session.active = false;
    }
    return session;
}

kernel::display::WindowStack stack_for(kernel::display::WindowSession session)
{
    kernel::display::WindowStack stack;
    if (!session.closed())
    {
        EXPECT_TRUE(stack.register_window(session));
    }
    return stack;
}

} // namespace

TEST(DesktopShellTest, TerminalActionShowsAndFocusesHiddenTerminal)
{
    const kernel::display::WindowSession terminal = terminal_session(false, false, false);
    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            terminal,
            stack_for(terminal));

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::ShowFocusAndRaise);
}

TEST(DesktopShellTest, TerminalActionCanFocusVisibleUnfocusedTerminal)
{
    const kernel::display::WindowSession terminal = terminal_session(true, false, false);
    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            terminal,
            stack_for(terminal));

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::FocusAndRaise);
}

TEST(DesktopShellTest, TerminalActionIgnoresFocusedOrClosedTerminal)
{
    const kernel::display::WindowSession focused_terminal = terminal_session(true, true, false);
    const kernel::display::WindowSession closed_terminal = terminal_session(false, false, true);
    const kernel::display::desktop_shell::AppLifecycleMutation focused =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            focused_terminal,
            stack_for(focused_terminal));
    const kernel::display::desktop_shell::AppLifecycleMutation closed =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            closed_terminal,
            stack_for(closed_terminal));

    EXPECT_EQ(focused, kernel::display::desktop_shell::AppLifecycleMutation::None);
    EXPECT_EQ(closed, kernel::display::desktop_shell::AppLifecycleMutation::None);
}

TEST(DesktopShellTest, TerminalActionRaisesFocusedWindowThatIsNotTopmost)
{
    kernel::display::WindowSession terminal = terminal_session(true, true, false);
    kernel::display::WindowStack stack;
    ASSERT_TRUE(stack.register_window(terminal));
    ASSERT_TRUE(stack.register_window(kernel::display::make_terminal_window_session(
        2,
        static_cast<kernel::display::AppSurfaceId>(2),
        {{400, 0, 320, 200}, {404, 16, 312, 180}},
        true,
        true,
        false,
        false)));

    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            terminal,
            stack);

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::FocusAndRaise);
}

TEST(DesktopShellTest, NoneActionDoesNothing)
{
    const kernel::display::WindowSession terminal = terminal_session(false, false, false);
    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::None,
            terminal,
            stack_for(terminal));

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::None);
}
