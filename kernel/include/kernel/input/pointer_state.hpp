#pragma once

#include <stdint.h>

namespace kernel::input
{

class PointerState
{
public:
    PointerState() = default;
    PointerState(uint64_t width, uint64_t height);

    void reset(uint64_t width, uint64_t height);
    void move_by(int16_t delta_x, int16_t delta_y);

    uint64_t x() const { return x_; }
    uint64_t y() const { return y_; }
    uint64_t width() const { return width_; }
    uint64_t height() const { return height_; }

private:
    [[nodiscard]] uint64_t clamp_x(int64_t value) const;
    [[nodiscard]] uint64_t clamp_y(int64_t value) const;

    uint64_t width_ = 0;
    uint64_t height_ = 0;
    uint64_t x_ = 0;
    uint64_t y_ = 0;
}; // end class PointerState

} // namespace kernel::input
