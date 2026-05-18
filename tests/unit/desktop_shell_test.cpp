#include <gtest/gtest.h>

#include "kernel/display/desktop_shell.hpp"

TEST(DesktopShellTest, TerminalActionShowsAndFocusesHiddenTerminal)
{
    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            {false, false, false});

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::ShowAndFocus);
}

TEST(DesktopShellTest, TerminalActionCanFocusVisibleUnfocusedTerminal)
{
    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            {true, false, false});

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::Focus);
}

TEST(DesktopShellTest, TerminalActionIgnoresFocusedOrClosedTerminal)
{
    const kernel::display::desktop_shell::AppLifecycleMutation focused =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            {true, true, false});
    const kernel::display::desktop_shell::AppLifecycleMutation closed =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus,
            {false, false, true});

    EXPECT_EQ(focused, kernel::display::desktop_shell::AppLifecycleMutation::None);
    EXPECT_EQ(closed, kernel::display::desktop_shell::AppLifecycleMutation::None);
}

TEST(DesktopShellTest, NoneActionDoesNothing)
{
    const kernel::display::desktop_shell::AppLifecycleMutation mutation =
        kernel::display::desktop_shell::ActionHandler::mutation_for(
            kernel::display::desktop_bar::DesktopShellAction::None,
            {false, false, false});

    EXPECT_EQ(mutation, kernel::display::desktop_shell::AppLifecycleMutation::None);
}
