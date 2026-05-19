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

} // namespace

TEST(DesktopShellTest, TerminalActionShowsAndFocusesHiddenTerminal)
{
    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            terminal_session(false, false, false));

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::ShowAndFocus);
}

TEST(DesktopShellTest, TerminalActionCanFocusVisibleUnfocusedTerminal)
{
    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            terminal_session(true, false, false));

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::Focus);
}

TEST(DesktopShellTest, TerminalActionIgnoresFocusedOrClosedTerminal)
{
    const kernel::display::desktop_shell::AppLifecycleMutation focused =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            terminal_session(true, true, false));
    const kernel::display::desktop_shell::AppLifecycleMutation closed =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            terminal_session(false, false, true));

    EXPECT_EQ(focused, kernel::display::desktop_shell::AppLifecycleMutation::None);
    EXPECT_EQ(closed, kernel::display::desktop_shell::AppLifecycleMutation::None);
}

TEST(DesktopShellTest, NoneActionDoesNothing)
{
    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::None,
            terminal_session(false, false, false));

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::None);
}
