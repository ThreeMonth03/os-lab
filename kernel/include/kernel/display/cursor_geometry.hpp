#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

class CursorGeometry
{
public:
    CursorGeometry() = default;
    CursorGeometry(Rect surface_bounds, uint64_t bitmap_width, uint64_t bitmap_height);

    Rect visible_rect(uint64_t hotspot_x, uint64_t hotspot_y) const;
    bool edge_push(uint64_t hotspot_x, uint64_t hotspot_y, int64_t delta_x, int64_t delta_y) const;

    Rect surface_bounds() const { return surface_bounds_; }
    uint64_t bitmap_width() const { return bitmap_width_; }
    uint64_t bitmap_height() const { return bitmap_height_; }

private:
    Rect surface_bounds_;
    uint64_t bitmap_width_ = 0;
    uint64_t bitmap_height_ = 0;
}; // end class CursorGeometry

} // namespace kernel::display
