#include "kernel/input/input_router.hpp"

namespace
{

kernel::input::InputRouter g_router;

} // namespace

namespace kernel::input
{

RoutedEvent InputRouter::route(const Event & event) const
{
    RoutedEvent routed;
    routed.event = event;

    switch (event.kind)
    {
    case EventKind::Key:
        if (focus_ == InputFocus::Shell)
        {
            routed.target = EventTarget::Shell;
        }
        else if (focus_ == InputFocus::GuiSurface)
        {
            routed.target = EventTarget::GuiSurface;
        }
        break;
    case EventKind::MouseMove:
        routed.target = EventTarget::Pointer;
        break;
    case EventKind::None:
        break;
    }

    return routed;
}

InputFocus focus()
{
    return g_router.focus();
}

void set_focus(InputFocus focus)
{
    g_router.set_focus(focus);
}

RoutedEvent route(const Event & event)
{
    return g_router.route(event);
}

} // namespace kernel::input
