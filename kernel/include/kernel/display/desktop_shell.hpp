#pragma once

#include "kernel/display/desktop_bar.hpp"
#include "kernel/display/window_session.hpp"
#include "kernel/display/window_stack.hpp"

namespace kernel::display::desktop_shell
{

enum class AppLifecycleMutation
{
    None,
    ShowFocusAndRaise,
    FocusAndRaise,
};

class ActionHandler
{
public:
    static AppLifecycleMutation mutation_for(desktop_bar::DesktopShellAction action,
                                             const WindowSession & terminal,
                                             const WindowStack & windows);
};

} // namespace kernel::display::desktop_shell
