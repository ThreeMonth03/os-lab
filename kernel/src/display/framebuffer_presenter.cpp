#include "kernel/display/framebuffer_presenter.hpp"

namespace kernel::display
{

namespace
{

Rect front_bounds(const Surface & surface)
{
    return {0, 0, surface.width(), surface.height()};
}

} // namespace

void FramebufferPresenter::reset(Surface & front_buffer, SceneBuffer & scene_buffer)
{
    front_buffer_ = &front_buffer;
    scene_buffer_ = &scene_buffer;
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

bool FramebufferPresenter::overlays_intersect(Rect rect) const
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

void FramebufferPresenter::put_presented_pixel(uint64_t x, uint64_t y)
{
    if (front_buffer_ == nullptr || scene_buffer_ == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < kMaxPresenterOverlays; ++index)
    {
        if (overlay_pixels_[index] == nullptr)
        {
            continue;
        }

        const PixelSample overlay = overlay_pixels_[index](x, y);
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

    if (!overlays_intersect(rect))
    {
        return copy_scene_to_front(rect);
    }

    for (uint64_t row = 0; row < rect.height; ++row)
    {
        const uint64_t y = rect.y + row;
        for (uint64_t column = 0; column < rect.width; ++column)
        {
            put_presented_pixel(rect.x + column, y);
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
    }
    return true;
}

} // namespace kernel::display
