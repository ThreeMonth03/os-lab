#include "kernel/display/display.hpp"

#include <stddef.h>

extern "C" void * memcpy(void * destination, const void * source, size_t size);
extern "C" void * memmove(void * destination, const void * source, size_t size);

namespace kernel::display
{
namespace
{

uint64_t saturating_end(uint64_t origin, uint64_t size)
{
    const uint64_t end = origin + size;
    if (end < origin)
    {
        return UINT64_MAX;
    }
    return end;
}

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
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

Surface::Surface(void * address, uint64_t width, uint64_t height, uint64_t pitch)
    : address_(address)
    , width_(width)
    , height_(height)
    , pitch_(pitch)
{
}

Color Surface::pixel(uint64_t x, uint64_t y) const
{
    if (!ready() || x >= width_ || y >= height_)
    {
        return {};
    }

    const auto * base = static_cast<const uint8_t *>(address_);
    const auto * pixel =
        reinterpret_cast<const uint32_t *>(base + (y * pitch_) + (x * sizeof(uint32_t)));
    return {*pixel};
}

void Surface::put_pixel(uint64_t x, uint64_t y, Color color)
{
    if (!ready() || x >= width_ || y >= height_)
    {
        return;
    }

    auto * base = static_cast<uint8_t *>(address_);
    auto * pixel = reinterpret_cast<uint32_t *>(base + (y * pitch_) + (x * sizeof(uint32_t)));
    *pixel = color.value;
}

void Surface::put_pixels(uint64_t x, uint64_t y, const uint32_t * pixels, size_t count)
{
    if (!ready() || pixels == nullptr || count == 0 || x >= width_ || y >= height_)
    {
        return;
    }

    const uint64_t available = width_ - x;
    if (count > available)
    {
        count = static_cast<size_t>(available);
    }
    if (count == 0)
    {
        return;
    }

    auto * base = static_cast<uint8_t *>(address_);
    auto * destination = reinterpret_cast<uint32_t *>(base + (y * pitch_) + (x * sizeof(uint32_t)));
    memcpy(destination, pixels, count * sizeof(uint32_t));
}

void Surface::fill_rect(Rect rect, Color color)
{
    rect = clip_rect(rect, width_, height_);
    if (!ready() || rect.empty())
    {
        return;
    }

    for (uint64_t row = 0; row < rect.height; ++row)
    {
        auto * base = static_cast<uint8_t *>(address_);
        auto * pixel = reinterpret_cast<uint32_t *>(base + ((rect.y + row) * pitch_) +
                                                    (rect.x * sizeof(uint32_t)));
        for (uint64_t column = 0; column < rect.width; ++column)
        {
            pixel[column] = color.value;
        }
    }
}

Rect Surface::copy_rect(Rect source, uint64_t destination_x, uint64_t destination_y)
{
    if (!ready())
    {
        return {};
    }

    const Rect bounds = {0, 0, width_, height_};
    source = clip_copy_source(source, bounds, destination_x, destination_y);
    source = clip_copy_destination(source, bounds, destination_x, destination_y);
    if (source.empty())
    {
        return {};
    }

    const bool copy_backwards =
        destination_y > source.y || (destination_y == source.y && destination_x > source.x);
    auto * base = static_cast<uint8_t *>(address_);

    if (copy_backwards)
    {
        for (uint64_t row = source.height; row > 0; --row)
        {
            const uint64_t current_row = row - 1;
            auto * destination =
                reinterpret_cast<uint32_t *>(base + ((destination_y + current_row) * pitch_) +
                                             (destination_x * sizeof(uint32_t)));
            const auto * source_row =
                reinterpret_cast<const uint32_t *>(base + ((source.y + current_row) * pitch_) +
                                                   (source.x * sizeof(uint32_t)));
            memmove(destination, source_row, source.width * sizeof(uint32_t));
        }
    }
    else
    {
        for (uint64_t row = 0; row < source.height; ++row)
        {
            auto * destination =
                reinterpret_cast<uint32_t *>(base + ((destination_y + row) * pitch_) +
                                             (destination_x * sizeof(uint32_t)));
            const auto * source_row =
                reinterpret_cast<const uint32_t *>(base + ((source.y + row) * pitch_) +
                                                   (source.x * sizeof(uint32_t)));
            memmove(destination, source_row, source.width * sizeof(uint32_t));
        }
    }

    return {destination_x, destination_y, source.width, source.height};
}

Rect clip_rect(Rect rect, uint64_t width, uint64_t height)
{
    if (rect.x >= width || rect.y >= height)
    {
        return {rect.x, rect.y, 0, 0};
    }

    const uint64_t available_width = width - rect.x;
    const uint64_t available_height = height - rect.y;
    if (rect.width > available_width)
    {
        rect.width = available_width;
    }
    if (rect.height > available_height)
    {
        rect.height = available_height;
    }

    return rect;
}

Rect bounding_rect(Rect lhs, Rect rhs)
{
    if (lhs.empty())
    {
        return rhs;
    }
    if (rhs.empty())
    {
        return lhs;
    }

    const uint64_t left = min_u64(lhs.x, rhs.x);
    const uint64_t top = min_u64(lhs.y, rhs.y);
    const uint64_t right = max_u64(saturating_end(lhs.x, lhs.width), saturating_end(rhs.x, rhs.width));
    const uint64_t bottom = max_u64(saturating_end(lhs.y, lhs.height), saturating_end(rhs.y, rhs.height));
    return {left, top, right - left, bottom - top};
}

Rect intersect_rect(Rect lhs, Rect rhs)
{
    if (lhs.empty() || rhs.empty())
    {
        return {};
    }

    const uint64_t left = max_u64(lhs.x, rhs.x);
    const uint64_t top = max_u64(lhs.y, rhs.y);
    const uint64_t right = min_u64(saturating_end(lhs.x, lhs.width), saturating_end(rhs.x, rhs.width));
    const uint64_t bottom = min_u64(saturating_end(lhs.y, lhs.height), saturating_end(rhs.y, rhs.height));
    if (right <= left || bottom <= top)
    {
        return {};
    }

    return {left, top, right - left, bottom - top};
}

} // namespace kernel::display
