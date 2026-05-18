#include "kernel/display/scroll_mapped_surface.hpp"

namespace kernel::display
{

namespace
{

[[nodiscard]] bool contains_rect(Rect outer, Rect inner)
{
    if (outer.empty() || inner.empty())
    {
        return false;
    }

    return inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.width <= outer.x + outer.width &&
           inner.y + inner.height <= outer.y + outer.height;
}

} // namespace

void ScrollMappedSurface::reset(BackingSurface & backing, Rect scroll_region)
{
    backing_ = &backing;
    scroll_region_ = intersect_rect(backing.bounds(), scroll_region);
    scroll_offset_ = 0;
}

void ScrollMappedSurface::reset_scroll()
{
    scroll_offset_ = 0;
}

void ScrollMappedSurface::scroll_up(uint64_t distance)
{
    if (scroll_region_.empty() || distance == 0)
    {
        return;
    }

    if (distance >= scroll_region_.height)
    {
        scroll_offset_ = 0;
        return;
    }

    scroll_offset_ = (scroll_offset_ + distance) % scroll_region_.height;
}

bool ScrollMappedSurface::contains(uint64_t x, uint64_t y) const
{
    return backing_ != nullptr && backing_->contains(x, y);
}

bool ScrollMappedSurface::in_scroll_region(uint64_t x, uint64_t y) const
{
    return !scroll_region_.empty() && x >= scroll_region_.x && y >= scroll_region_.y &&
           x < scroll_region_.x + scroll_region_.width &&
           y < scroll_region_.y + scroll_region_.height;
}

uint64_t ScrollMappedSurface::mapped_y(uint64_t y) const
{
    if (!in_scroll_region(scroll_region_.x, y) || scroll_offset_ == 0)
    {
        return y;
    }

    const uint64_t local_y = y - scroll_region_.y;
    return scroll_region_.y + ((local_y + scroll_offset_) % scroll_region_.height);
}

Color ScrollMappedSurface::pixel_mapped(uint64_t x, uint64_t y) const
{
    const Rect backing_bounds = backing_->bounds();
    const uint64_t physical_y = in_scroll_region(x, y) ? mapped_y(y) : y;
    const uint32_t * row = backing_->row_pixels(physical_y);
    if (row == nullptr)
    {
        return {};
    }
    return {row[x - backing_bounds.x]};
}

Color ScrollMappedSurface::pixel(uint64_t x, uint64_t y) const
{
    if (!contains(x, y))
    {
        return {};
    }
    return pixel_mapped(x, y);
}

PixelSample ScrollMappedSurface::sample(uint64_t x, uint64_t y) const
{
    if (!contains(x, y))
    {
        return transparent_pixel();
    }
    return opaque_pixel(pixel_mapped(x, y));
}

const uint32_t * ScrollMappedSurface::row_pixels(uint64_t y) const
{
    if (backing_ == nullptr)
    {
        return nullptr;
    }
    const Rect backing_bounds = backing_->bounds();
    if (y < backing_bounds.y || y >= backing_bounds.y + backing_bounds.height)
    {
        return nullptr;
    }
    return backing_->row_pixels(mapped_y(y));
}

void ScrollMappedSurface::put_pixel(uint64_t x, uint64_t y, Color color)
{
    if (backing_ == nullptr)
    {
        return;
    }
    backing_->put_pixel(x, in_scroll_region(x, y) ? mapped_y(y) : y, color);
}

void ScrollMappedSurface::fill_mapped_scroll_rect(Rect rect, Color color)
{
    const uint64_t mapped_top = mapped_y(rect.y);
    const uint64_t scroll_bottom = scroll_region_.y + scroll_region_.height;
    if (mapped_top + rect.height <= scroll_bottom)
    {
        backing_->fill_rect({rect.x, mapped_top, rect.width, rect.height}, color);
        return;
    }

    const uint64_t first_height = scroll_bottom - mapped_top;
    backing_->fill_rect({rect.x, mapped_top, rect.width, first_height}, color);
    backing_->fill_rect({rect.x,
                         scroll_region_.y,
                         rect.width,
                         rect.height - first_height},
                        color);
}

void ScrollMappedSurface::fill_rect(Rect rect, Color color)
{
    rect = intersect_rect(rect, bounds());
    if (rect.empty())
    {
        return;
    }

    if (scroll_offset_ == 0 || intersect_rect(rect, scroll_region_).empty())
    {
        backing_->fill_rect(rect, color);
        return;
    }

    if (contains_rect(scroll_region_, rect))
    {
        fill_mapped_scroll_rect(rect, color);
        return;
    }

    for (uint64_t row = 0; row < rect.height; ++row)
    {
        const uint64_t y = rect.y + row;
        for (uint64_t column = 0; column < rect.width; ++column)
        {
            put_pixel(rect.x + column, y, color);
        }
    }
}

} // namespace kernel::display
