#include <gtest/gtest.h>
#include "kernel/base/fixed_queue.hpp"
#include "kernel/base/fixed_vector.hpp"

namespace
{

class MoveOnly
{
public:
    explicit MoveOnly(int value)
        : value_(value)
    {
    }

    MoveOnly(const MoveOnly &) = delete;
    MoveOnly & operator=(const MoveOnly &) = delete;

    MoveOnly(MoveOnly && other)
        : value_(other.value_)
    {
        other.value_ = -1;
    }

    MoveOnly & operator=(MoveOnly && other)
    {
        if (this != &other)
        {
            value_ = other.value_;
            other.value_ = -1;
        }

        return *this;
    }

    [[nodiscard]] int value() const { return value_; }

private:
    int value_ = 0;
};

TEST(FixedVectorTest, StoresValuesWithoutHeap)
{
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

TEST(FixedVectorTest, MovesElementsWithoutHeap)
{
    kernel::FixedVector<MoveOnly, 3> values;
    MoveOnly second(2);

    ASSERT_NE(values.emplace_back(1), nullptr);
    EXPECT_TRUE(values.push_back(static_cast<MoveOnly &&>(second)));

    kernel::FixedVector<MoveOnly, 3> moved(static_cast<kernel::FixedVector<MoveOnly, 3> &&>(values));
    EXPECT_TRUE(values.empty());
    ASSERT_EQ(moved.size(), 2u);
    EXPECT_EQ(moved[0].value(), 1);
    EXPECT_EQ(moved[1].value(), 2);

    kernel::FixedVector<MoveOnly, 3> assigned;
    ASSERT_NE(assigned.emplace_back(9), nullptr);
    assigned = static_cast<kernel::FixedVector<MoveOnly, 3> &&>(moved);

    EXPECT_TRUE(moved.empty());
    ASSERT_EQ(assigned.size(), 2u);
    EXPECT_EQ(assigned[0].value(), 1);
    EXPECT_EQ(assigned[1].value(), 2);
}

TEST(FixedQueueTest, PushesAndPopsInOrder)
{
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

TEST(FixedQueueTest, ReportsFullAndRejectsNewest)
{
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

TEST(FixedQueueTest, WrapsAround)
{
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

TEST(FixedQueueTest, ClearEmptiesQueue)
{
    kernel::FixedQueue<int, 2> values;

    EXPECT_TRUE(values.push(1));
    EXPECT_TRUE(values.push(2));
    values.clear();
    EXPECT_TRUE(values.empty());
    EXPECT_FALSE(values.full());
    EXPECT_EQ(values.available(), values.capacity());
}

} // namespace
