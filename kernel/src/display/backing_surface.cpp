#include "kernel/display/backing_surface.hpp"

namespace kernel::display
{

bool backing_surface_required_bytes(Rect bounds, size_t & bytes)
{
    bytes = 0;
    if (bounds.empty())
    {
        return false;
    }

    constexpr size_t kBytesPerPixel = sizeof(uint32_t);
    if (bounds.width > static_cast<size_t>(-1) / bounds.height)
    {
        return false;
    }

    const size_t pixels = bounds.width * bounds.height;
    if (pixels > static_cast<size_t>(-1) / kBytesPerPixel)
    {
        return false;
    }

    bytes = pixels * kBytesPerPixel;
    return true;
}

BackingSurface::BackingSurface(uint32_t * pixels, Rect bounds, uint64_t stride_pixels)
    : pixels_(pixels)
    , bounds_(bounds)
    , stride_pixels_(stride_pixels)
{
}

bool BackingSurface::contains(uint64_t x, uint64_t y) const
{
    return ready() && x >= bounds_.x && y >= bounds_.y && x < bounds_.x + bounds_.width &&
           y < bounds_.y + bounds_.height;
}

Color BackingSurface::pixel(uint64_t x, uint64_t y) const
{
    if (!contains(x, y))
    {
        return {};
    }

    const uint64_t local_x = x - bounds_.x;
    const uint64_t local_y = y - bounds_.y;
    return {pixels_[(local_y * stride_pixels_) + local_x]};
}

PixelSample BackingSurface::sample(uint64_t x, uint64_t y) const
{
    if (!contains(x, y))
    {
        return transparent_pixel();
    }

    return opaque_pixel(pixel(x, y));
}

void BackingSurface::put_pixel(uint64_t x, uint64_t y, Color color)
{
    if (!contains(x, y))
    {
        return;
    }

    const uint64_t local_x = x - bounds_.x;
    const uint64_t local_y = y - bounds_.y;
    pixels_[(local_y * stride_pixels_) + local_x] = color.value;
}

void BackingSurface::fill_rect(Rect rect, Color color)
{
    rect = intersect_rect(bounds_, rect);
    if (!ready() || rect.empty())
    {
        return;
    }

    for (uint64_t row = 0; row < rect.height; ++row)
    {
        for (uint64_t column = 0; column < rect.width; ++column)
        {
            put_pixel(rect.x + column, rect.y + row, color);
        }
    }
}

} // namespace kernel::display
