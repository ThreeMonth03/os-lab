#include <gtest/gtest.h>

#include "kernel/display/desktop_shell.hpp"

TEST(DesktopShellTest, TerminalActionMapsToWindowManagerCommand)
{
    const kernel::display::desktop_shell::WindowCommand command =
        kernel::display::desktop_shell::ActionHandler::command_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus);

    EXPECT_EQ(command, kernel::display::desktop_shell::WindowCommand::TerminalShowFocusRaise);
}

TEST(DesktopShellTest, NoneActionDoesNothing)
{
    const kernel::display::desktop_shell::WindowCommand command =
        kernel::display::desktop_shell::ActionHandler::command_for(
            kernel::display::desktop_bar::DesktopShellAction::None);

    EXPECT_EQ(command, kernel::display::desktop_shell::WindowCommand::None);
}
