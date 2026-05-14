#include <gtest/gtest.h>
#include "kernel/input/pointer_state.hpp"

namespace {

TEST(PointerStateTest, InitializesAtCenteredPosition) {
    const kernel::PointerState pointer(100, 80, 10, 16);

    EXPECT_EQ(pointer.x(), 45u);
    EXPECT_EQ(pointer.y(), 32u);
}

TEST(PointerStateTest, AppliesMouseDeltasWithInvertedY) {
    kernel::PointerState pointer(100, 80, 10, 16);

    pointer.move_by(5, 7);

    EXPECT_EQ(pointer.x(), 50u);
    EXPECT_EQ(pointer.y(), 25u);
}

TEST(PointerStateTest, ClampsToBounds) {
    kernel::PointerState pointer(100, 80, 10, 16);

    pointer.move_by(-200, 200);
    EXPECT_EQ(pointer.x(), 0u);
    EXPECT_EQ(pointer.y(), 0u);

    pointer.move_by(300, -300);
    EXPECT_EQ(pointer.x(), 90u);
    EXPECT_EQ(pointer.y(), 64u);
}

TEST(PointerStateTest, HandlesPointerLargerThanBounds) {
    kernel::PointerState pointer(8, 8, 10, 16);

    pointer.move_by(10, -10);

    EXPECT_EQ(pointer.x(), 0u);
    EXPECT_EQ(pointer.y(), 0u);
}

} // namespace
