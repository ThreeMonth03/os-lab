#include "kernel/input/input.hpp"

#include "kernel/arch/x86_64/interrupt_guard.hpp"
#include "kernel/base/placement_new.hpp"
#include "kernel/input/input_queue.hpp"
#include "kernel/input/mouse.hpp"

namespace
{

constexpr size_t kEventQueueCapacity = 32;
constexpr int kMaxPollsPerPump = 16;

using EventQueue = kernel::input::InputQueue<kEventQueueCapacity>;

alignas(EventQueue) unsigned char g_event_queue_storage[sizeof(EventQueue)] = {};
bool g_event_queue_initialized = false;

EventQueue & event_queue()
{
    auto * queue = reinterpret_cast<EventQueue *>(g_event_queue_storage);
    if (!g_event_queue_initialized)
    {
        new (queue) EventQueue();
        g_event_queue_initialized = true;
    }

    return *queue;
}

bool queue_full()
{
    kernel::arch::x86_64::InterruptGuard guard;
    EventQueue & events = event_queue();
    return events.full();
}

bool poll_keyboard()
{
    if (kernel::keyboard::input_mode() == kernel::keyboard::InputMode::Irq)
    {
        return false;
    }

    kernel::keyboard::KeyEvent key;
    if (!kernel::keyboard::poll_key(key))
    {
        return false;
    }

    kernel::input::Event event;
    event.kind = kernel::input::EventKind::Key;
    event.key = key;
    event.key_source = kernel::input::KeyEventSource::PollingFallback;
    return kernel::input::enqueue(event);
}

bool poll_mouse()
{
    kernel::mouse::MouseEvent mouse;
    if (!kernel::mouse::poll(mouse))
    {
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
    return kernel::input::enqueue(event);
}

} // namespace

namespace kernel::input
{

void pump()
{
    for (int poll_count = 0; poll_count < kMaxPollsPerPump && !queue_full(); ++poll_count)
    {
        const bool pushed_key = poll_keyboard();
        const bool pushed_mouse = poll_mouse();
        if (!pushed_key && !pushed_mouse)
        {
            return;
        }
    }
}

bool enqueue(const Event & event)
{
    kernel::arch::x86_64::InterruptGuard guard;
    return event_queue().push(event);
}

bool poll(Event & event)
{
    event = {};
    {
        kernel::arch::x86_64::InterruptGuard guard;
        if (event_queue().pop(event))
        {
            return true;
        }
    }

    pump();
    {
        kernel::arch::x86_64::InterruptGuard guard;
        return event_queue().pop(event);
    }
}

Stats stats()
{
    kernel::arch::x86_64::InterruptGuard guard;
    Stats result = event_queue().stats();
    result.keyboard_mode = kernel::keyboard::input_mode();
    return result;
}

} // namespace kernel::input
