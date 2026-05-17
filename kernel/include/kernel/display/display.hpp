#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel::display
{

struct Color
{
    uint32_t value = 0;
};

struct Rect
{
    uint64_t x = 0;
    uint64_t y = 0;
    uint64_t width = 0;
    uint64_t height = 0;

    bool empty() const { return width == 0 || height == 0; }
};

class Surface
{
public:
    Surface() = default;
    Surface(void * address, uint64_t width, uint64_t height, uint64_t pitch);

    bool ready() const { return address_ != nullptr && width_ > 0 && height_ > 0; }
    uint64_t width() const { return width_; }
    uint64_t height() const { return height_; }
    uint64_t pitch() const { return pitch_; }

    [[nodiscard]] Color pixel(uint64_t x, uint64_t y) const;
    void put_pixel(uint64_t x, uint64_t y, Color color);
    void put_pixels(uint64_t x, uint64_t y, const uint32_t * pixels, size_t count);
    void fill_rect(Rect rect, Color color);
    [[nodiscard]] Rect copy_rect(Rect source, uint64_t destination_x, uint64_t destination_y);

private:
    void * address_ = nullptr;
    uint64_t width_ = 0;
    uint64_t height_ = 0;
    uint64_t pitch_ = 0;
};

[[nodiscard]] Rect clip_rect(Rect rect, uint64_t width, uint64_t height);
[[nodiscard]] Rect bounding_rect(Rect lhs, Rect rhs);
[[nodiscard]] Rect intersect_rect(Rect lhs, Rect rhs);

} // namespace kernel::display
