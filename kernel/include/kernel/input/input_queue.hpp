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

    [[nodiscard]] bool push(const Event & event)
    {
        record_event(event);
        if (events_.full())
        {
            ++dropped_events_;
            return false;
        }
        return events_.push(event);
    }

    [[nodiscard]] bool pop(Event & event) { return events_.pop(event); }
    [[nodiscard]] bool full() const { return events_.full(); }
    [[nodiscard]] bool empty() const { return events_.empty(); }

    [[nodiscard]] Stats stats() const
    {
        return {
            key_events_,
            mouse_move_events_,
            dropped_events_,
            events_.size(),
            events_.capacity(),
            events_.available(),
        };
    }

private:
    void record_event(const Event & event)
    {
        switch (event.kind)
        {
        case EventKind::Key:
            ++key_events_;
            break;
        case EventKind::MouseMove:
            ++mouse_move_events_;
            break;
        case EventKind::None:
            break;
        }
    }

    FixedQueue<Event, Capacity> events_;
    uint64_t key_events_ = 0;
    uint64_t mouse_move_events_ = 0;
    uint64_t dropped_events_ = 0;
};

} // namespace kernel::input
