#pragma once

#include <stdint.h>

#include "kernel/display/backing_surface.hpp"
#include "kernel/display/display.hpp"

namespace kernel::display
{

class SceneBuffer
{
public:
    SceneBuffer() = default;
    SceneBuffer(uint32_t * pixels, Rect bounds, uint64_t stride_pixels);

    bool ready() const { return backing_.ready(); }
    Rect bounds() const { return backing_.bounds(); }
    uint64_t width() const { return backing_.width(); }
    uint64_t height() const { return backing_.height(); }
    uint64_t stride_pixels() const { return backing_.stride_pixels(); }

    [[nodiscard]] bool contains(uint64_t x, uint64_t y) const;
    [[nodiscard]] Color pixel(uint64_t x, uint64_t y) const;
    [[nodiscard]] PixelSample sample(uint64_t x, uint64_t y) const;
    void put_pixel(uint64_t x, uint64_t y, Color color);
    void fill_rect(Rect rect, Color color);
    [[nodiscard]] Rect copy_rect(Rect source, uint64_t destination_x, uint64_t destination_y);

private:
    BackingSurface backing_;
}; // end class SceneBuffer

} // namespace kernel::display
