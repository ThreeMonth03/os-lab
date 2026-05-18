#pragma once

#include <stdint.h>

#include "kernel/display/backing_surface.hpp"
#include "kernel/display/display.hpp"

namespace kernel::display
{

class ScrollMappedSurface
{
public:
    ScrollMappedSurface() = default;
    ScrollMappedSurface(BackingSurface & backing, Rect scroll_region)
    {
        reset(backing, scroll_region);
    }

    void reset(BackingSurface & backing, Rect scroll_region);
    void reset_scroll();
    void scroll_up(uint64_t distance);

    bool ready() const { return backing_ != nullptr && backing_->ready(); }
    Rect bounds() const { return backing_ == nullptr ? Rect{} : backing_->bounds(); }
    Rect scroll_region() const { return scroll_region_; }
    uint64_t scroll_offset() const { return scroll_offset_; }

    [[nodiscard]] bool contains(uint64_t x, uint64_t y) const;
    [[nodiscard]] Color pixel(uint64_t x, uint64_t y) const;
    [[nodiscard]] PixelSample sample(uint64_t x, uint64_t y) const;
    [[nodiscard]] const uint32_t * row_pixels(uint64_t y) const;
    void put_pixel(uint64_t x, uint64_t y, Color color);
    void fill_rect(Rect rect, Color color);

private:
    [[nodiscard]] bool in_scroll_region(uint64_t x, uint64_t y) const;
    [[nodiscard]] Color pixel_mapped(uint64_t x, uint64_t y) const;
    void fill_mapped_scroll_rect(Rect rect, Color color);
    [[nodiscard]] uint64_t mapped_y(uint64_t y) const;

    BackingSurface * backing_ = nullptr;
    Rect scroll_region_;
    uint64_t scroll_offset_ = 0;
}; // end class ScrollMappedSurface

} // namespace kernel::display
