#include "kernel/display/cursor_geometry.hpp"

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

} // namespace

namespace kernel::display
{

CursorGeometry::CursorGeometry(Rect surface_bounds, uint64_t bitmap_width, uint64_t bitmap_height)
    : surface_bounds_(surface_bounds)
    , bitmap_width_(bitmap_width)
    , bitmap_height_(bitmap_height)
{
}

Rect CursorGeometry::visible_rect(uint64_t hotspot_x, uint64_t hotspot_y) const
{
    if (surface_bounds_.empty() || bitmap_width_ == 0 || bitmap_height_ == 0)
    {
        return {};
    }

    const uint64_t left = max_u64(hotspot_x, surface_bounds_.x);
    const uint64_t top = max_u64(hotspot_y, surface_bounds_.y);
    const uint64_t right = min_u64(saturating_end(hotspot_x, bitmap_width_),
                                   saturating_end(surface_bounds_.x, surface_bounds_.width));
    const uint64_t bottom = min_u64(saturating_end(hotspot_y, bitmap_height_),
                                    saturating_end(surface_bounds_.y, surface_bounds_.height));
    if (right <= left || bottom <= top)
    {
        return {};
    }

    return {left, top, right - left, bottom - top};
}

bool CursorGeometry::edge_push(uint64_t hotspot_x, uint64_t hotspot_y, int64_t delta_x, int64_t delta_y) const
{
    if (surface_bounds_.empty())
    {
        return false;
    }

    const uint64_t left = surface_bounds_.x;
    const uint64_t top = surface_bounds_.y;
    const uint64_t right = saturating_end(surface_bounds_.x, surface_bounds_.width) - 1;
    const uint64_t bottom = saturating_end(surface_bounds_.y, surface_bounds_.height) - 1;

    return (delta_x < 0 && hotspot_x <= left) || (delta_x > 0 && hotspot_x >= right) ||
           (delta_y < 0 && hotspot_y <= top) || (delta_y > 0 && hotspot_y >= bottom);
}

} // namespace kernel::display
