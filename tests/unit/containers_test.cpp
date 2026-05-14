#include <gtest/gtest.h>
#include "kernel/fixed_queue.hpp"
#include "kernel/fixed_vector.hpp"

namespace {

TEST(FixedVectorTest, StoresValuesWithoutHeap) {
    kernel::FixedVector<int, 3> values;

    EXPECT_TRUE(values.empty());
    EXPECT_TRUE(values.push_back(1));
    EXPECT_TRUE(values.push_back(2));
    EXPECT_TRUE(values.push_back(3));
    EXPECT_FALSE(values.push_back(4));
    EXPECT_TRUE(values.full());
    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(values[1], 2);
    EXPECT_TRUE(values.pop_back());
    EXPECT_EQ(values.size(), 2u);
    values.clear();
    EXPECT_TRUE(values.empty());
}

TEST(FixedQueueTest, PushesAndPopsInOrder) {
    kernel::FixedQueue<int, 3> values;
    int value = 0;

    EXPECT_TRUE(values.empty());
    EXPECT_TRUE(values.push(1));
    EXPECT_TRUE(values.push(2));
    EXPECT_EQ(values.size(), 2u);

    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_FALSE(values.pop(value));
    EXPECT_TRUE(values.empty());
}

TEST(FixedQueueTest, ReportsFullAndRejectsNewest) {
    kernel::FixedQueue<int, 2> values;
    int value = 0;

    EXPECT_EQ(values.available(), 2u);
    EXPECT_TRUE(values.push(1));
    EXPECT_EQ(values.available(), 1u);
    EXPECT_TRUE(values.push(2));
    EXPECT_TRUE(values.full());
    EXPECT_EQ(values.available(), 0u);
    EXPECT_FALSE(values.push(3));

    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_EQ(values.available(), 1u);
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_EQ(values.available(), 2u);
}

TEST(FixedQueueTest, WrapsAround) {
    kernel::FixedQueue<int, 3> values;
    int value = 0;

    EXPECT_TRUE(values.push(1));
    EXPECT_TRUE(values.push(2));
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(values.push(3));
    EXPECT_TRUE(values.push(4));
    EXPECT_TRUE(values.full());

    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 3);
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 4);
    EXPECT_TRUE(values.empty());
}

TEST(FixedQueueTest, ClearEmptiesQueue) {
    kernel::FixedQueue<int, 2> values;

    EXPECT_TRUE(values.push(1));
    EXPECT_TRUE(values.push(2));
    values.clear();
    EXPECT_TRUE(values.empty());
    EXPECT_FALSE(values.full());
    EXPECT_EQ(values.available(), values.capacity());
}

} // namespace
