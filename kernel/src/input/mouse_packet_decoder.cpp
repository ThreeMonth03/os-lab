#include "kernel/mouse_packet_decoder.hpp"

namespace {

constexpr uint8_t kAlwaysSet = 0x08;
constexpr uint8_t kLeftButton = 0x01;
constexpr uint8_t kRightButton = 0x02;
constexpr uint8_t kMiddleButton = 0x04;
constexpr uint8_t kXSign = 0x10;
constexpr uint8_t kYSign = 0x20;
constexpr uint8_t kXOverflow = 0x40;
constexpr uint8_t kYOverflow = 0x80;

int16_t sign_extend(uint8_t value, bool negative) {
    uint16_t extended = value;
    if (negative) {
        extended = static_cast<uint16_t>(extended | 0xff00u);
    }

    return static_cast<int16_t>(extended);
}

} // namespace

namespace kernel::mouse {

bool MousePacketDecoder::decode(uint8_t byte, MousePacket& packet) {
    packet = {};

    if (index_ == 0 && (byte & kAlwaysSet) == 0) {
        return false;
    }

    bytes_[index_] = byte;
    ++index_;
    if (index_ < 3) {
        return false;
    }

    const uint8_t flags = bytes_[0];
    packet.left_button = (flags & kLeftButton) != 0;
    packet.right_button = (flags & kRightButton) != 0;
    packet.middle_button = (flags & kMiddleButton) != 0;
    packet.x_overflow = (flags & kXOverflow) != 0;
    packet.y_overflow = (flags & kYOverflow) != 0;
    packet.delta_x = sign_extend(bytes_[1], (flags & kXSign) != 0);
    packet.delta_y = sign_extend(bytes_[2], (flags & kYSign) != 0);

    reset();
    return true;
}

void MousePacketDecoder::reset() {
    bytes_[0] = 0;
    bytes_[1] = 0;
    bytes_[2] = 0;
    index_ = 0;
}

} // namespace kernel::mouse
