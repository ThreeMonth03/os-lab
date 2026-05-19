#include "kernel/display/desktop_shell.hpp"

namespace kernel::display::desktop_shell
{

AppLifecycleMutation ActionHandler::mutation_for(desktop_bar::DesktopShellAction action,
                                                 const WindowSession & terminal)
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
            return AppLifecycleMutation::ShowAndFocus;
        }
        return terminal.focused ? AppLifecycleMutation::None : AppLifecycleMutation::Focus;
    }
    return AppLifecycleMutation::None;
}

} // namespace kernel::display::desktop_shell
