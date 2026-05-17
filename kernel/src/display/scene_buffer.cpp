#include "kernel/display/scene_buffer.hpp"

namespace kernel::display
{

SceneBuffer::SceneBuffer(uint32_t * pixels, Rect bounds, uint64_t stride_pixels)
    : backing_(pixels, bounds, stride_pixels)
{
}

bool SceneBuffer::contains(uint64_t x, uint64_t y) const
{
    return backing_.contains(x, y);
}

Color SceneBuffer::pixel(uint64_t x, uint64_t y) const
{
    return backing_.pixel(x, y);
}

PixelSample SceneBuffer::sample(uint64_t x, uint64_t y) const
{
    return backing_.sample(x, y);
}

void SceneBuffer::put_pixel(uint64_t x, uint64_t y, Color color)
{
    backing_.put_pixel(x, y, color);
}

void SceneBuffer::fill_rect(Rect rect, Color color)
{
    backing_.fill_rect(rect, color);
}

Rect SceneBuffer::copy_rect(Rect source, uint64_t destination_x, uint64_t destination_y)
{
    return backing_.copy_rect(source, destination_x, destination_y);
}

} // namespace kernel::display
