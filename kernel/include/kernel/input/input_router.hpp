#pragma once

#include "kernel/input/input.hpp"
#include "kernel/input/input_types.hpp"

namespace kernel::input
{

struct RoutedEvent
{
    EventTarget target = EventTarget::None;
    Event event = {};
};

class InputRouter
{
public:
    InputRouter() = default;

    [[nodiscard]] InputFocus focus() const { return focus_; }
    void set_focus(InputFocus focus) { focus_ = focus; }

    [[nodiscard]] RoutedEvent route(const Event & event) const;

private:
    InputFocus focus_ = InputFocus::Shell;
};

} // namespace kernel::input
