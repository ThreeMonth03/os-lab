#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

enum class PixelSampleKind
{
    Transparent,
    Opaque,
};

struct PixelSample
{
    PixelSampleKind kind = PixelSampleKind::Transparent;
    Color color;

    bool opaque() const { return kind == PixelSampleKind::Opaque; }
};

[[nodiscard]] constexpr PixelSample transparent_pixel()
{
    return {};
}

[[nodiscard]] constexpr PixelSample opaque_pixel(Color color)
{
    return {PixelSampleKind::Opaque, color};
}

[[nodiscard]] bool backing_surface_required_bytes(Rect bounds, size_t & bytes);

class BackingSurface
{
public:
    BackingSurface() = default;
    BackingSurface(uint32_t * pixels, Rect bounds, uint64_t stride_pixels);

    bool ready() const { return pixels_ != nullptr && !bounds_.empty() && stride_pixels_ >= bounds_.width; }
    Rect bounds() const { return bounds_; }
    uint64_t width() const { return bounds_.width; }
    uint64_t height() const { return bounds_.height; }
    uint64_t stride_pixels() const { return stride_pixels_; }

    [[nodiscard]] bool contains(uint64_t x, uint64_t y) const;
    [[nodiscard]] Color pixel(uint64_t x, uint64_t y) const;
    [[nodiscard]] PixelSample sample(uint64_t x, uint64_t y) const;
    void put_pixel(uint64_t x, uint64_t y, Color color);
    void fill_rect(Rect rect, Color color);
    [[nodiscard]] Rect copy_rect(Rect source, uint64_t destination_x, uint64_t destination_y);

private:
    uint32_t * pixels_ = nullptr;
    Rect bounds_;
    uint64_t stride_pixels_ = 0;
}; // end class BackingSurface

} // namespace kernel::display
