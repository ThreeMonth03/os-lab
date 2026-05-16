#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

enum class PixelFormat : uint8_t
{
    Rgba8888,
    Bgra8888,
};

struct RgbaColor
{
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    uint8_t alpha = 0;

    bool operator==(const RgbaColor & other) const = default;
};

struct ColorLayout
{
    uint8_t red_shift = 16;
    uint8_t green_shift = 8;
    uint8_t blue_shift = 0;
};

constexpr ColorLayout xrgb8888_color_layout()
{
    return {};
}

class ImageView
{
public:
    ImageView() = default;
    ImageView(const void * pixels,
              uint64_t width,
              uint64_t height,
              uint64_t stride,
              PixelFormat format);

    bool valid() const;
    const uint8_t * pixels() const { return pixels_; }
    uint64_t width() const { return width_; }
    uint64_t height() const { return height_; }
    uint64_t stride() const { return stride_; }
    PixelFormat format() const { return format_; }
    Rect bounds_at(uint64_t x, uint64_t y) const;
    RgbaColor pixel(uint64_t x, uint64_t y) const;

private:
    const uint8_t * pixels_ = nullptr;
    uint64_t width_ = 0;
    uint64_t height_ = 0;
    uint64_t stride_ = 0;
    PixelFormat format_ = PixelFormat::Rgba8888;
};

[[nodiscard]] Color pack_color(RgbaColor color, ColorLayout layout);
[[nodiscard]] Rect image_blit_region(ImageView image,
                                     Rect surface_bounds,
                                     uint64_t destination_x,
                                     uint64_t destination_y,
                                     Rect dirty_rect);
void blit_image(Surface & surface,
                ImageView image,
                uint64_t destination_x,
                uint64_t destination_y,
                Rect dirty_rect,
                ColorLayout layout);

} // namespace kernel::display
