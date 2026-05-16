#include "kernel/display/image_view.hpp"

namespace kernel::display
{
namespace
{

constexpr uint64_t kBytesPerPixel = 4;

bool valid_format(PixelFormat format)
{
    return format == PixelFormat::Rgba8888 || format == PixelFormat::Bgra8888;
}

bool row_stride_can_hold_width(uint64_t width, uint64_t stride)
{
    return width <= (UINT64_MAX / kBytesPerPixel) && stride >= width * kBytesPerPixel;
}

uint32_t shifted_channel(uint8_t value, uint8_t shift)
{
    return static_cast<uint32_t>(value) << shift;
}

} // namespace

ImageView::ImageView(const void * pixels,
                     uint64_t width,
                     uint64_t height,
                     uint64_t stride,
                     PixelFormat format)
    : pixels_(static_cast<const uint8_t *>(pixels))
    , width_(width)
    , height_(height)
    , stride_(stride)
    , format_(format)
{
}

bool ImageView::valid() const
{
    return pixels_ != nullptr && width_ > 0 && height_ > 0 && valid_format(format_) &&
           row_stride_can_hold_width(width_, stride_);
}

Rect ImageView::bounds_at(uint64_t x, uint64_t y) const
{
    if (!valid())
    {
        return {};
    }
    return {x, y, width_, height_};
}

RgbaColor ImageView::pixel(uint64_t x, uint64_t y) const
{
    if (!valid() || x >= width_ || y >= height_)
    {
        return {};
    }

    const uint8_t * pixel = pixels_ + (y * stride_) + (x * kBytesPerPixel);
    switch (format_)
    {
    case PixelFormat::Rgba8888:
        return {pixel[0], pixel[1], pixel[2], pixel[3]};
    case PixelFormat::Bgra8888:
        return {pixel[2], pixel[1], pixel[0], pixel[3]};
    }

    return {};
}

Color pack_color(RgbaColor color, ColorLayout layout)
{
    return {
        shifted_channel(color.red, layout.red_shift) |
            shifted_channel(color.green, layout.green_shift) |
            shifted_channel(color.blue, layout.blue_shift),
    };
}

Rect image_blit_region(ImageView image,
                       Rect surface_bounds,
                       uint64_t destination_x,
                       uint64_t destination_y,
                       Rect dirty_rect)
{
    if (!image.valid() || surface_bounds.empty() || dirty_rect.empty())
    {
        return {};
    }

    return intersect_rect(intersect_rect(image.bounds_at(destination_x, destination_y),
                                         surface_bounds),
                          dirty_rect);
}

void blit_image(Surface & surface,
                ImageView image,
                uint64_t destination_x,
                uint64_t destination_y,
                Rect dirty_rect,
                ColorLayout layout)
{
    const Rect region = image_blit_region(image,
                                          {0, 0, surface.width(), surface.height()},
                                          destination_x,
                                          destination_y,
                                          dirty_rect);
    if (!surface.ready() || region.empty())
    {
        return;
    }

    for (uint64_t row = 0; row < region.height; ++row)
    {
        for (uint64_t column = 0; column < region.width; ++column)
        {
            const uint64_t target_x = region.x + column;
            const uint64_t target_y = region.y + row;
            const uint64_t source_x = target_x - destination_x;
            const uint64_t source_y = target_y - destination_y;
            surface.put_pixel(target_x, target_y, pack_color(image.pixel(source_x, source_y), layout));
        }
    }
}

} // namespace kernel::display
