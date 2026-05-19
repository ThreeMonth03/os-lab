#include "kernel/display/desktop_shell.hpp"

namespace kernel::display::desktop_shell
{

WindowCommand ActionHandler::command_for(desktop_bar::DesktopShellAction action)
{
    switch (action)
    {
    case desktop_bar::DesktopShellAction::None:
        return WindowCommand::None;
    case desktop_bar::DesktopShellAction::TerminalShowFocus:
        return WindowCommand::TerminalShowFocusRaise;
    }
    return WindowCommand::None;
}

} // namespace kernel::display::desktop_shell
