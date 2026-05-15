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
        Stats result;
        result.key_events = key_events_;
        result.keyboard_irq_events = keyboard_irq_events_;
        result.keyboard_polling_fallback_events = keyboard_polling_fallback_events_;
        result.mouse_move_events = mouse_move_events_;
        result.mouse_irq_events = mouse_irq_events_;
        result.mouse_polling_fallback_events = mouse_polling_fallback_events_;
        result.dropped_events = dropped_events_;
        result.queued_events = events_.size();
        result.queue_capacity = events_.capacity();
        result.queue_available = events_.available();
        return result;
    }

private:
    void record_event(const Event & event)
    {
        switch (event.kind)
        {
        case EventKind::Key:
            ++key_events_;
            record_key_source(event.source);
            break;
        case EventKind::MouseMove:
            ++mouse_move_events_;
            record_mouse_source(event.source);
            break;
        case EventKind::None:
            break;
        }
    }

    void record_key_source(EventSource source)
    {
        switch (source)
        {
        case EventSource::Irq:
            ++keyboard_irq_events_;
            break;
        case EventSource::PollingFallback:
            ++keyboard_polling_fallback_events_;
            break;
        case EventSource::Unknown:
            break;
        }
    }

    void record_mouse_source(EventSource source)
    {
        switch (source)
        {
        case EventSource::Irq:
            ++mouse_irq_events_;
            break;
        case EventSource::PollingFallback:
            ++mouse_polling_fallback_events_;
            break;
        case EventSource::Unknown:
            break;
        }
    }

    FixedQueue<Event, Capacity> events_;
    uint64_t key_events_ = 0;
    uint64_t keyboard_irq_events_ = 0;
    uint64_t keyboard_polling_fallback_events_ = 0;
    uint64_t mouse_move_events_ = 0;
    uint64_t mouse_irq_events_ = 0;
    uint64_t mouse_polling_fallback_events_ = 0;
    uint64_t dropped_events_ = 0;
};

} // namespace kernel::input
