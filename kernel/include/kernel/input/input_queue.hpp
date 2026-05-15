#pragma once

#include <stddef.h>

#include "kernel/base/fixed_queue.hpp"
#include "kernel/input/input.hpp"

namespace kernel::input
{

template <size_t Capacity>
class InputQueue
{
public:
    InputQueue() = default;

    [[nodiscard]] bool push(const Event & event) { return events_.push(event); }
    [[nodiscard]] bool pop(Event & event) { return events_.pop(event); }
    bool full() const { return events_.full(); }
    bool empty() const { return events_.empty(); }
    size_t size() const { return events_.size(); }
    size_t capacity() const { return events_.capacity(); }
    size_t available() const { return events_.available(); }

private:
    FixedQueue<Event, Capacity> events_;
};

} // namespace kernel::input
