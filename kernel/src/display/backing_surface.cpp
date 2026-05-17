#include "kernel/display/backing_surface.hpp"

namespace kernel::display
{

namespace
{

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

uint64_t saturating_end(uint64_t origin, uint64_t size)
{
    const uint64_t end = origin + size;
    return end < origin ? UINT64_MAX : end;
}

Rect clip_copy_source(Rect source, Rect bounds, uint64_t & destination_x, uint64_t & destination_y)
{
    if (source.empty() || bounds.empty())
    {
        return {};
    }

    const uint64_t source_left = max_u64(source.x, bounds.x);
    const uint64_t source_top = max_u64(source.y, bounds.y);
    const uint64_t source_right = min_u64(saturating_end(source.x, source.width),
                                          saturating_end(bounds.x, bounds.width));
    const uint64_t source_bottom = min_u64(saturating_end(source.y, source.height),
                                           saturating_end(bounds.y, bounds.height));
    if (source_right <= source_left || source_bottom <= source_top)
    {
        return {};
    }

    destination_x += source_left - source.x;
    destination_y += source_top - source.y;
    return {source_left, source_top, source_right - source_left, source_bottom - source_top};
}

Rect clip_copy_destination(Rect source, Rect bounds, uint64_t & destination_x, uint64_t & destination_y)
{
    if (source.empty() || bounds.empty())
    {
        return {};
    }

    uint64_t left_trim = 0;
    uint64_t top_trim = 0;
    if (destination_x < bounds.x)
    {
        left_trim = bounds.x - destination_x;
    }
    if (destination_y < bounds.y)
    {
        top_trim = bounds.y - destination_y;
    }
    if (left_trim >= source.width || top_trim >= source.height)
    {
        return {};
    }

    source.x += left_trim;
    source.y += top_trim;
    source.width -= left_trim;
    source.height -= top_trim;
    destination_x += left_trim;
    destination_y += top_trim;

    const uint64_t destination_right = min_u64(saturating_end(destination_x, source.width),
                                               saturating_end(bounds.x, bounds.width));
    const uint64_t destination_bottom = min_u64(saturating_end(destination_y, source.height),
                                                saturating_end(bounds.y, bounds.height));
    if (destination_right <= destination_x || destination_bottom <= destination_y)
    {
        return {};
    }

    source.width = destination_right - destination_x;
    source.height = destination_bottom - destination_y;
    return source;
}

} // namespace

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

Rect BackingSurface::copy_rect(Rect source, uint64_t destination_x, uint64_t destination_y)
{
    if (!ready())
    {
        return {};
    }

    source = clip_copy_source(source, bounds_, destination_x, destination_y);
    source = clip_copy_destination(source, bounds_, destination_x, destination_y);
    if (source.empty())
    {
        return {};
    }

    const uint64_t source_local_x = source.x - bounds_.x;
    const uint64_t source_local_y = source.y - bounds_.y;
    const uint64_t destination_local_x = destination_x - bounds_.x;
    const uint64_t destination_local_y = destination_y - bounds_.y;
    const bool copy_backwards =
        destination_local_y > source_local_y ||
        (destination_local_y == source_local_y && destination_local_x > source_local_x);

    if (copy_backwards)
    {
        for (uint64_t row = source.height; row > 0; --row)
        {
            const uint64_t current_row = row - 1;
            for (uint64_t column = source.width; column > 0; --column)
            {
                const uint64_t current_column = column - 1;
                pixels_[((destination_local_y + current_row) * stride_pixels_) + destination_local_x +
                        current_column] =
                    pixels_[((source_local_y + current_row) * stride_pixels_) + source_local_x +
                            current_column];
            }
        }
    }
    else
    {
        for (uint64_t row = 0; row < source.height; ++row)
        {
            for (uint64_t column = 0; column < source.width; ++column)
            {
                pixels_[((destination_local_y + row) * stride_pixels_) + destination_local_x +
                        column] =
                    pixels_[((source_local_y + row) * stride_pixels_) + source_local_x + column];
            }
        }
    }

    return {destination_x, destination_y, source.width, source.height};
}

} // namespace kernel::display
