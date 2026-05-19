#pragma once

#include "kernel/display/desktop_bar.hpp"

namespace kernel::display::desktop_shell
{

enum class WindowCommand
{
    None,
    TerminalShowFocusRaise,
    DummyDebugAppShowFocusRaise,
};

class ActionHandler
{
public:
    static WindowCommand command_for(desktop_bar::DesktopShellAction action);
};

} // namespace kernel::display::desktop_shell
