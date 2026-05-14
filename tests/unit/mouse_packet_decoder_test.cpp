#include <gtest/gtest.h>
#include "kernel/input/mouse_packet_decoder.hpp"

namespace {

TEST(MousePacketDecoderTest, DecodesMovementAndButtons) {
    kernel::mouse::MousePacketDecoder decoder;
    kernel::mouse::MousePacket packet;

    EXPECT_FALSE(decoder.decode(0x0b, packet));
    EXPECT_FALSE(decoder.decode(5, packet));
    EXPECT_TRUE(decoder.decode(6, packet));

    EXPECT_EQ(packet.delta_x, 5);
    EXPECT_EQ(packet.delta_y, 6);
    EXPECT_TRUE(packet.left_button);
    EXPECT_TRUE(packet.right_button);
    EXPECT_FALSE(packet.middle_button);
    EXPECT_FALSE(packet.x_overflow);
    EXPECT_FALSE(packet.y_overflow);
}

TEST(MousePacketDecoderTest, DecodesNegativeMovement) {
    kernel::mouse::MousePacketDecoder decoder;
    kernel::mouse::MousePacket packet;

    EXPECT_FALSE(decoder.decode(0x38, packet));
    EXPECT_FALSE(decoder.decode(0xfe, packet));
    EXPECT_TRUE(decoder.decode(0xfd, packet));

    EXPECT_EQ(packet.delta_x, -2);
    EXPECT_EQ(packet.delta_y, -3);
}

TEST(MousePacketDecoderTest, TracksOverflowBits) {
    kernel::mouse::MousePacketDecoder decoder;
    kernel::mouse::MousePacket packet;

    EXPECT_FALSE(decoder.decode(0xc8, packet));
    EXPECT_FALSE(decoder.decode(0, packet));
    EXPECT_TRUE(decoder.decode(0, packet));

    EXPECT_TRUE(packet.x_overflow);
    EXPECT_TRUE(packet.y_overflow);
}

TEST(MousePacketDecoderTest, ResynchronizesOnInvalidFirstByte) {
    kernel::mouse::MousePacketDecoder decoder;
    kernel::mouse::MousePacket packet;

    EXPECT_FALSE(decoder.decode(0x00, packet));
    EXPECT_FALSE(decoder.decode(0x08, packet));
    EXPECT_FALSE(decoder.decode(1, packet));
    EXPECT_TRUE(decoder.decode(2, packet));

    EXPECT_EQ(packet.delta_x, 1);
    EXPECT_EQ(packet.delta_y, 2);
}

} // namespace
