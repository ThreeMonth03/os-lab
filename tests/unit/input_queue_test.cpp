#include <gtest/gtest.h>

#include "kernel/input/input_queue.hpp"
#include "kernel/input/input_stats_tracker.hpp"

namespace
{

kernel::input::Event key_event(char character,
                               kernel::input::EventSource source = kernel::input::EventSource::Unknown)
{
    kernel::input::Event event;
    event.kind = kernel::input::EventKind::Key;
    event.source = source;
    event.key.key = kernel::keyboard::Key::Character;
    event.key.character = character;
    event.key.pressed = true;
    return event;
}

kernel::input::Event mouse_event(int16_t delta_x,
                                 int16_t delta_y,
                                 kernel::input::EventSource source = kernel::input::EventSource::Unknown)
{
    kernel::input::Event event;
    event.kind = kernel::input::EventKind::MouseMove;
    event.source = source;
    event.mouse_move.delta_x = delta_x;
    event.mouse_move.delta_y = delta_y;
    return event;
}

TEST(InputQueueTest, PushesAndPopsEventsInOrder)
{
    kernel::input::InputQueue<3> queue;

    EXPECT_TRUE(queue.push(key_event('a')));
    EXPECT_TRUE(queue.push(mouse_event(2, -1)));

    kernel::input::Event event;
    ASSERT_TRUE(queue.pop(event));
    EXPECT_EQ(event.kind, kernel::input::EventKind::Key);
    EXPECT_EQ(event.key.character, 'a');

    ASSERT_TRUE(queue.pop(event));
    EXPECT_EQ(event.kind, kernel::input::EventKind::MouseMove);
    EXPECT_EQ(event.mouse_move.delta_x, 2);
    EXPECT_EQ(event.mouse_move.delta_y, -1);

    EXPECT_FALSE(queue.pop(event));
}

TEST(InputQueueTest, RejectsNewestWhenFull)
{
    kernel::input::InputQueue<1> queue;

    EXPECT_TRUE(queue.push(key_event('a', kernel::input::EventSource::Irq)));
    EXPECT_FALSE(queue.push(mouse_event(4, 5, kernel::input::EventSource::Irq)));
    EXPECT_TRUE(queue.full());
    EXPECT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue.capacity(), 1u);
    EXPECT_EQ(queue.available(), 0u);

    kernel::input::Event event;
    ASSERT_TRUE(queue.pop(event));
    EXPECT_EQ(event.kind, kernel::input::EventKind::Key);
    EXPECT_EQ(event.key.character, 'a');
}

TEST(InputStatsTrackerTest, CountsInputSourcesAndQueueSnapshot)
{
    kernel::input::InputStatsTracker tracker;

    tracker.record_event(key_event('a', kernel::input::EventSource::Irq));
    tracker.record_event(key_event('b', kernel::input::EventSource::PollingFallback));
    tracker.record_event(mouse_event(4, 5, kernel::input::EventSource::Irq));
    tracker.record_event(mouse_event(6, 7, kernel::input::EventSource::PollingFallback));
    tracker.record_dropped_event();

    const kernel::input::Stats stats = tracker.snapshot(3, 4, 1);
    EXPECT_EQ(stats.key_events, 2u);
    EXPECT_EQ(stats.keyboard_irq_events, 1u);
    EXPECT_EQ(stats.keyboard_polling_fallback_events, 1u);
    EXPECT_EQ(stats.mouse_move_events, 2u);
    EXPECT_EQ(stats.mouse_irq_events, 1u);
    EXPECT_EQ(stats.mouse_polling_fallback_events, 1u);
    EXPECT_EQ(stats.dropped_events, 1u);
    EXPECT_EQ(stats.queued_events, 3u);
    EXPECT_EQ(stats.queue_capacity, 4u);
    EXPECT_EQ(stats.queue_available, 1u);
}

TEST(InputStatsTrackerTest, IgnoresUnknownSourcesForSourceBreakdown)
{
    kernel::input::InputStatsTracker tracker;

    tracker.record_event(key_event('x'));
    tracker.record_event(mouse_event(1, 1));

    const kernel::input::Stats stats = tracker.snapshot(2, 3, 1);
    EXPECT_EQ(stats.key_events, 1u);
    EXPECT_EQ(stats.keyboard_irq_events, 0u);
    EXPECT_EQ(stats.keyboard_polling_fallback_events, 0u);
    EXPECT_EQ(stats.mouse_move_events, 1u);
    EXPECT_EQ(stats.mouse_irq_events, 0u);
    EXPECT_EQ(stats.mouse_polling_fallback_events, 0u);
}

TEST(InputQueueTest, ReportsStatsSnapshot)
{
    kernel::input::InputQueue<4> queue;

    EXPECT_TRUE(queue.push(key_event('x', kernel::input::EventSource::Irq)));
    EXPECT_TRUE(queue.push(mouse_event(1, 1, kernel::input::EventSource::Irq)));
    EXPECT_TRUE(queue.push(mouse_event(2, 2, kernel::input::EventSource::PollingFallback)));
    EXPECT_TRUE(queue.push(key_event('y', kernel::input::EventSource::PollingFallback)));

    EXPECT_EQ(queue.size(), 4u);
    EXPECT_EQ(queue.capacity(), 4u);
    EXPECT_EQ(queue.available(), 0u);

    kernel::input::Event event;
    EXPECT_TRUE(queue.pop(event));
    EXPECT_EQ(queue.size(), 3u);
    EXPECT_EQ(queue.available(), 1u);
}

} // namespace
