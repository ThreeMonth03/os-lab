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
    cursor_pixel_ = pixel_reader;
    cursor_bounds_ = bounds_reader;
}

Rect FramebufferPresenter::cursor_bounds() const
{
    if (cursor_bounds_ == nullptr)
    {
        return {};
    }
    return cursor_bounds_();
}

void FramebufferPresenter::put_presented_pixel(uint64_t x, uint64_t y)
{
    if (front_buffer_ == nullptr || scene_buffer_ == nullptr)
    {
        return;
    }

    if (cursor_pixel_ != nullptr)
    {
        const PixelSample cursor = cursor_pixel_(x, y);
        if (cursor.opaque())
        {
            front_buffer_->put_pixel(x, y, cursor.color);
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

    if (intersect_rect(rect, cursor_bounds()).empty())
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

bool FramebufferPresenter::copy_scene_rect(Rect source,
                                           uint64_t destination_x,
                                           uint64_t destination_y)
{
    if (!ready())
    {
        return false;
    }

    const Rect cursor = cursor_bounds();
    if (!cursor.empty() && !copy_scene_to_front(cursor))
    {
        return false;
    }

    const Rect copied = scene_buffer_->copy_rect(source, destination_x, destination_y);
    if (copied.empty())
    {
        return false;
    }
    if (!copy_scene_to_front(copied))
    {
        return false;
    }
    return cursor.empty() || present_rect(cursor);
}

} // namespace kernel::display
