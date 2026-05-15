#include <gtest/gtest.h>

#include "kernel/input/input_queue.hpp"

namespace
{

kernel::input::Event key_event(char character)
{
    kernel::input::Event event;
    event.kind = kernel::input::EventKind::Key;
    event.key.key = kernel::keyboard::Key::Character;
    event.key.character = character;
    event.key.pressed = true;
    return event;
}

kernel::input::Event mouse_event(int16_t delta_x, int16_t delta_y)
{
    kernel::input::Event event;
    event.kind = kernel::input::EventKind::MouseMove;
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

TEST(InputQueueTest, DropsNewestWhenFullAndCountsEvents)
{
    kernel::input::InputQueue<1> queue;

    EXPECT_TRUE(queue.push(key_event('a')));
    EXPECT_FALSE(queue.push(mouse_event(4, 5)));

    const kernel::input::Stats stats = queue.stats();
    EXPECT_EQ(stats.key_events, 1u);
    EXPECT_EQ(stats.mouse_move_events, 1u);
    EXPECT_EQ(stats.dropped_events, 1u);
    EXPECT_EQ(stats.queued_events, 1u);
    EXPECT_EQ(stats.queue_capacity, 1u);
    EXPECT_EQ(stats.queue_available, 0u);

    kernel::input::Event event;
    ASSERT_TRUE(queue.pop(event));
    EXPECT_EQ(event.kind, kernel::input::EventKind::Key);
    EXPECT_EQ(event.key.character, 'a');
}

TEST(InputQueueTest, ReportsStatsSnapshot)
{
    kernel::input::InputQueue<4> queue;

    EXPECT_TRUE(queue.push(key_event('x')));
    EXPECT_TRUE(queue.push(mouse_event(1, 1)));
    EXPECT_TRUE(queue.push(key_event('y')));

    kernel::input::Stats stats = queue.stats();
    EXPECT_EQ(stats.key_events, 2u);
    EXPECT_EQ(stats.mouse_move_events, 1u);
    EXPECT_EQ(stats.dropped_events, 0u);
    EXPECT_EQ(stats.queued_events, 3u);
    EXPECT_EQ(stats.queue_capacity, 4u);
    EXPECT_EQ(stats.queue_available, 1u);

    kernel::input::Event event;
    EXPECT_TRUE(queue.pop(event));
    stats = queue.stats();
    EXPECT_EQ(stats.queued_events, 2u);
    EXPECT_EQ(stats.queue_available, 2u);
}

} // namespace
