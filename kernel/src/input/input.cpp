#include "kernel/input/input.hpp"

#include "kernel/base/fixed_queue.hpp"
#include "kernel/input/mouse.hpp"
#include "kernel/base/placement_new.hpp"

namespace {

constexpr size_t kEventQueueCapacity = 32;
constexpr int kMaxPollsPerPump = 16;

using EventQueue = kernel::FixedQueue<kernel::input::Event, kEventQueueCapacity>;

alignas(EventQueue) unsigned char g_event_queue_storage[sizeof(EventQueue)] = {};
bool g_event_queue_initialized = false;
uint64_t g_key_events = 0;
uint64_t g_mouse_move_events = 0;
uint64_t g_dropped_events = 0;

EventQueue& event_queue() {
    auto* queue = reinterpret_cast<EventQueue*>(g_event_queue_storage);
    if (!g_event_queue_initialized) {
        new (queue) EventQueue();
        g_event_queue_initialized = true;
    }

    return *queue;
}

bool push_event(const kernel::input::Event& event) {
    EventQueue& events = event_queue();
    if (events.full()) {
        ++g_dropped_events;
        return false;
    }

    return events.push(event);
}

bool poll_keyboard() {
    kernel::keyboard::KeyEvent key;
    if (!kernel::keyboard::poll_key(key)) {
        return false;
    }

    kernel::input::Event event;
    event.kind = kernel::input::EventKind::Key;
    event.key = key;
    ++g_key_events;
    return push_event(event);
}

bool poll_mouse() {
    kernel::mouse::MouseEvent mouse;
    if (!kernel::mouse::poll(mouse)) {
        return false;
    }

    kernel::input::Event event;
    event.kind = kernel::input::EventKind::MouseMove;
    event.mouse_move.delta_x = mouse.delta_x;
    event.mouse_move.delta_y = mouse.delta_y;
    event.mouse_move.left_button = mouse.left_button;
    event.mouse_move.right_button = mouse.right_button;
    event.mouse_move.middle_button = mouse.middle_button;
    event.mouse_move.x_overflow = mouse.x_overflow;
    event.mouse_move.y_overflow = mouse.y_overflow;
    ++g_mouse_move_events;
    return push_event(event);
}

} // namespace

namespace kernel::input {

void pump() {
    EventQueue& events = event_queue();
    for (int poll_count = 0; poll_count < kMaxPollsPerPump && !events.full(); ++poll_count) {
        const bool pushed_key = poll_keyboard();
        const bool pushed_mouse = poll_mouse();
        if (!pushed_key && !pushed_mouse) {
            return;
        }
    }
}

bool poll(Event& event) {
    event = {};
    EventQueue& events = event_queue();
    if (events.pop(event)) {
        return true;
    }

    pump();
    return events.pop(event);
}

Stats stats() {
    EventQueue& events = event_queue();
    return {g_key_events,  g_mouse_move_events, g_dropped_events,
            events.size(), events.capacity(),   events.available()};
}

} // namespace kernel::input
