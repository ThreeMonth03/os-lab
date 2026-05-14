#pragma once

#include <stdint.h>

namespace kernel
{

class PointerState
{
public:
    PointerState() = default;
    PointerState(uint64_t width, uint64_t height, uint64_t pointer_width, uint64_t pointer_height);

    void reset(uint64_t width, uint64_t height, uint64_t pointer_width, uint64_t pointer_height);
    void move_by(int16_t delta_x, int16_t delta_y);

    [[nodiscard]] uint64_t x() const { return x_; }
    [[nodiscard]] uint64_t y() const { return y_; }
    [[nodiscard]] uint64_t width() const { return width_; }
    [[nodiscard]] uint64_t height() const { return height_; }
    [[nodiscard]] uint64_t pointer_width() const { return pointer_width_; }
    [[nodiscard]] uint64_t pointer_height() const { return pointer_height_; }

private:
    [[nodiscard]] uint64_t clamp_x(int64_t value) const;
    [[nodiscard]] uint64_t clamp_y(int64_t value) const;

    uint64_t width_ = 0;
    uint64_t height_ = 0;
    uint64_t pointer_width_ = 0;
    uint64_t pointer_height_ = 0;
    uint64_t x_ = 0;
    uint64_t y_ = 0;
};

} // namespace kernel
