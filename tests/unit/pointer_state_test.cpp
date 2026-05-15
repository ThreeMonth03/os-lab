#include <gtest/gtest.h>
#include "kernel/input/pointer_state.hpp"

namespace
{

TEST(PointerStateTest, InitializesAtCenteredPosition)
{
    const kernel::input::PointerState pointer(100, 80);

    EXPECT_EQ(pointer.x(), 50u);
    EXPECT_EQ(pointer.y(), 40u);
}

TEST(PointerStateTest, AppliesMouseDeltasWithInvertedY)
{
    kernel::input::PointerState pointer(100, 80);

    pointer.move_by(5, 7);

    EXPECT_EQ(pointer.x(), 55u);
    EXPECT_EQ(pointer.y(), 33u);
}

TEST(PointerStateTest, ClampsToBounds)
{
    kernel::input::PointerState pointer(100, 80);

    pointer.move_by(-200, 200);
    EXPECT_EQ(pointer.x(), 0u);
    EXPECT_EQ(pointer.y(), 0u);

    pointer.move_by(300, -300);
    EXPECT_EQ(pointer.x(), 99u);
    EXPECT_EQ(pointer.y(), 79u);
}

TEST(PointerStateTest, HandlesEmptyBounds)
{
    kernel::input::PointerState pointer(0, 0);

    pointer.move_by(10, -10);

    EXPECT_EQ(pointer.x(), 0u);
    EXPECT_EQ(pointer.y(), 0u);
}

} // namespace
