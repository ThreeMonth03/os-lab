#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel::input::mouse
{

struct MousePacket
{
    int16_t delta_x = 0;
    int16_t delta_y = 0;
    bool left_button = false;
    bool right_button = false;
    bool middle_button = false;
    bool x_overflow = false;
    bool y_overflow = false;
};

class MousePacketDecoder
{
public:
    [[nodiscard]] bool decode(uint8_t byte, MousePacket & packet);
    void reset();

private:
    uint8_t bytes_[3] = {};
    size_t index_ = 0;
};

} // namespace kernel::input::mouse
