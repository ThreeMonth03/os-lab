#include "kernel/input/input_router.hpp"

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
        break;
    case EventKind::MouseMove:
        routed.target = EventTarget::Pointer;
        break;
    case EventKind::None:
        break;
    }

    return routed;
}

} // namespace kernel::input
