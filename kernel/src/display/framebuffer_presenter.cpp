#include "kernel/display/framebuffer_presenter.hpp"

namespace kernel::display
{

namespace
{

uint64_t rect_area(Rect rect)
{
    return rect.width * rect.height;
}

Rect front_bounds(const Surface & surface)
{
    return {0, 0, surface.width(), surface.height()};
}

} // namespace

void FramebufferPresenter::reset(Surface & front_buffer, SceneBuffer & scene_buffer)
{
    front_buffer_ = &front_buffer;
    scene_buffer_ = &scene_buffer;
    reset_stats();
}

void FramebufferPresenter::set_cursor_overlay(CursorPixelReader pixel_reader,
                                              CursorBoundsReader bounds_reader)
{
    set_overlay(0, pixel_reader, bounds_reader);
}

void FramebufferPresenter::set_overlay(size_t index,
                                       CursorPixelReader pixel_reader,
                                       CursorBoundsReader bounds_reader)
{
    if (index >= kMaxPresenterOverlays)
    {
        return;
    }

    overlay_pixels_[index] = pixel_reader;
    overlay_bounds_[index] = bounds_reader;
}

Rect FramebufferPresenter::overlay_bounds(size_t index) const
{
    if (index >= kMaxPresenterOverlays || overlay_bounds_[index] == nullptr)
    {
        return {};
    }
    return overlay_bounds_[index]();
}

void FramebufferPresenter::put_presented_pixel(uint64_t x, uint64_t y)
{
    if (front_buffer_ == nullptr || scene_buffer_ == nullptr)
    {
        return;
    }

    for (CursorPixelReader overlay_pixel : overlay_pixels_)
    {
        if (overlay_pixel == nullptr)
        {
            continue;
        }

        const PixelSample overlay = overlay_pixel(x, y);
        if (overlay.opaque())
        {
            front_buffer_->put_pixel(x, y, overlay.color);
            return;
        }
    }
    front_buffer_->put_pixel(x, y, scene_buffer_->pixel(x, y));
}

bool FramebufferPresenter::present_rect(Rect rect)
{
    if (!ready())
    {
        return false;
    }

    rect = intersect_rect(rect, front_bounds(*front_buffer_));
    rect = intersect_rect(rect, scene_buffer_->bounds());
    if (rect.empty())
    {
        return true;
    }

    ++stats_.present_call_count;
    ++stats_.present_rect_count;
    const uint64_t area = rect_area(rect);
    stats_.total_presented_pixels += area;
    if (area > stats_.largest_present_rect_area)
    {
        stats_.largest_present_rect_area = area;
    }

    if (!copy_scene_to_front(rect))
    {
        return false;
    }

    for (size_t index = 0; index < kMaxPresenterOverlays; ++index)
    {
        const Rect overlay = intersect_rect(rect, overlay_bounds(index));
        if (!present_overlay_rect(overlay))
        {
            return false;
        }
    }

    return true;
}

bool FramebufferPresenter::present_scroll(Rect rect, uint64_t distance)
{
    if (!ready())
    {
        return false;
    }

    rect = intersect_rect(rect, front_bounds(*front_buffer_));
    rect = intersect_rect(rect, scene_buffer_->bounds());
    if (rect.empty() || distance == 0)
    {
        return true;
    }
    if (distance >= rect.height)
    {
        return present_rect(rect);
    }

    ++stats_.present_call_count;
    ++stats_.present_scroll_count;

    const Rect copied = {
        rect.x,
        rect.y,
        rect.width,
        rect.height - distance,
    };
    const uint64_t area = rect_area(copied);
    stats_.total_presented_pixels += area;

    if (!copy_front_scroll(rect, distance))
    {
        if (!copy_scene_to_front(copied))
        {
            return false;
        }
    }

    for (size_t index = 0; index < kMaxPresenterOverlays; ++index)
    {
        const Rect overlay = intersect_rect(copied, overlay_bounds(index));
        if (!present_overlay_rect(overlay))
        {
            return false;
        }
    }

    return true;
}

bool FramebufferPresenter::overlay_intersects(Rect rect) const
{
    for (size_t index = 0; index < kMaxPresenterOverlays; ++index)
    {
        if (!intersect_rect(rect, overlay_bounds(index)).empty())
        {
            return true;
        }
    }
    return false;
}

bool FramebufferPresenter::present_overlay_rect(Rect rect)
{
    if (!ready())
    {
        return false;
    }

    rect = intersect_rect(rect, front_bounds(*front_buffer_));
    rect = intersect_rect(rect, scene_buffer_->bounds());
    if (rect.empty())
    {
        return true;
    }

    for (uint64_t row = 0; row < rect.height; ++row)
    {
        const uint64_t y = rect.y + row;
        for (uint64_t column = 0; column < rect.width; ++column)
        {
            put_presented_pixel(rect.x + column, y);
            ++stats_.overlay_blend_pixels;
        }
    }
    return true;
}

bool FramebufferPresenter::copy_scene_to_front(Rect rect)
{
    if (!ready())
    {
        return false;
    }

    rect = intersect_rect(rect, front_bounds(*front_buffer_));
    rect = intersect_rect(rect, scene_buffer_->bounds());
    if (rect.empty())
    {
        return true;
    }

    const Rect scene_bounds = scene_buffer_->bounds();
    for (uint64_t row = 0; row < rect.height; ++row)
    {
        const uint64_t y = rect.y + row;
        const uint32_t * pixels = scene_buffer_->row_pixels(y);
        if (pixels == nullptr)
        {
            return false;
        }

        front_buffer_->put_pixels(rect.x, y, pixels + (rect.x - scene_bounds.x), rect.width);
        stats_.fast_path_copy_pixels += rect.width;
    }
    return true;
}

bool FramebufferPresenter::copy_front_scroll(Rect rect, uint64_t distance)
{
    if (!ready() || rect.empty() || distance == 0 || distance >= rect.height)
    {
        return false;
    }

    const Rect source = {
        rect.x,
        rect.y + distance,
        rect.width,
        rect.height - distance,
    };
    const Rect destination = {
        rect.x,
        rect.y,
        rect.width,
        rect.height - distance,
    };
    if (overlay_intersects(source) || overlay_intersects(destination))
    {
        return false;
    }

    const Rect copied = front_buffer_->copy_rect(source, destination.x, destination.y);
    if (copied.empty())
    {
        return false;
    }

    stats_.front_scroll_copy_pixels += copied.width * copied.height;
    return true;
}

void FramebufferPresenter::reset_stats()
{
    stats_ = {};
}

} // namespace kernel::display
