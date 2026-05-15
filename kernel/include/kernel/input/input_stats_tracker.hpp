#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/input/input.hpp"

namespace kernel::input
{

class InputStatsTracker
{
public:
    InputStatsTracker() = default;

    void record_event(const Event & event)
    {
        switch (event.kind)
        {
        case EventKind::Key:
            ++key_events_;
            record_keyboard_source(event.source);
            break;
        case EventKind::MouseMove:
            ++mouse_move_events_;
            record_mouse_source(event.source);
            break;
        case EventKind::None:
            break;
        }
    }

    void record_dropped_event() { ++dropped_events_; }

    [[nodiscard]] Stats snapshot(size_t queued_events, size_t queue_capacity, size_t queue_available) const
    {
        Stats result;
        result.key_events = key_events_;
        result.keyboard_irq_events = keyboard_irq_events_;
        result.keyboard_polling_fallback_events = keyboard_polling_fallback_events_;
        result.mouse_move_events = mouse_move_events_;
        result.mouse_irq_events = mouse_irq_events_;
        result.mouse_polling_fallback_events = mouse_polling_fallback_events_;
        result.dropped_events = dropped_events_;
        result.queued_events = queued_events;
        result.queue_capacity = queue_capacity;
        result.queue_available = queue_available;
        return result;
    }

private:
    void record_keyboard_source(EventSource source)
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

    uint64_t key_events_ = 0;
    uint64_t keyboard_irq_events_ = 0;
    uint64_t keyboard_polling_fallback_events_ = 0;
    uint64_t mouse_move_events_ = 0;
    uint64_t mouse_irq_events_ = 0;
    uint64_t mouse_polling_fallback_events_ = 0;
    uint64_t dropped_events_ = 0;
};

} // namespace kernel::input
