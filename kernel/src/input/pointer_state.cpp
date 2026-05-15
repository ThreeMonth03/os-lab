#include "kernel/input/pointer_state.hpp"

namespace
{

uint64_t max_position(uint64_t extent)
{
    return extent > 0 ? extent - 1 : 0;
}

uint64_t clamp_position(int64_t value, uint64_t max)
{
    if (value <= 0)
    {
        return 0;
    }
    if (static_cast<uint64_t>(value) > max)
    {
        return max;
    }

    return static_cast<uint64_t>(value);
}

} // namespace

namespace kernel::input
{

PointerState::PointerState(uint64_t width, uint64_t height)
{
    reset(width, height);
}

void PointerState::reset(uint64_t width, uint64_t height)
{
    width_ = width;
    height_ = height;
    x_ = width_ / 2;
    y_ = height_ / 2;
}

void PointerState::move_by(int16_t delta_x, int16_t delta_y)
{
    x_ = clamp_x(static_cast<int64_t>(x_) + delta_x);
    y_ = clamp_y(static_cast<int64_t>(y_) - delta_y);
}

uint64_t PointerState::clamp_x(int64_t value) const
{
    return clamp_position(value, max_position(width_));
}

uint64_t PointerState::clamp_y(int64_t value) const
{
    return clamp_position(value, max_position(height_));
}

} // namespace kernel::input
