#pragma once

#include "kernel/display/desktop_bar.hpp"
#include "kernel/display/window_session.hpp"

namespace kernel::display::desktop_shell
{

enum class AppLifecycleMutation
{
    None,
    ShowAndFocus,
    Focus,
};

class ActionHandler
{
public:
    static AppLifecycleMutation mutation_for(desktop_bar::DesktopShellAction action,
                                             const WindowSession & terminal);
};

} // namespace kernel::display::desktop_shell
