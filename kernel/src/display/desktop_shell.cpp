#include "kernel/display/desktop_shell.hpp"

namespace kernel::display::desktop_shell
{

AppLifecycleMutation ActionHandler::mutation_for(desktop_bar::DesktopShellAction action,
                                                 const WindowSession & terminal,
                                                 const WindowStack & windows)
{
    switch (action)
    {
    case desktop_bar::DesktopShellAction::None:
        return AppLifecycleMutation::None;
    case desktop_bar::DesktopShellAction::TerminalShowFocus:
        if (terminal.closed())
        {
            return AppLifecycleMutation::None;
        }
        if (!terminal.visible())
        {
            return AppLifecycleMutation::ShowFocusAndRaise;
        }
        const bool already_selected =
            terminal.focused && terminal.active && windows.topmost_visible_window() == terminal.id;
        return already_selected ? AppLifecycleMutation::None
                                : AppLifecycleMutation::FocusAndRaise;
    }
    return AppLifecycleMutation::None;
}

} // namespace kernel::display::desktop_shell
