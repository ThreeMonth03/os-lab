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

void FramebufferPresenter::copy_front_rect(Rect source,
                                           uint64_t destination_x,
                                           uint64_t destination_y)
{
    if (front_buffer_ == nullptr)
    {
        return;
    }

    source = intersect_rect(source, front_bounds(*front_buffer_));
    source = intersect_rect(source, scene_buffer_->bounds());
    if (source.empty() || destination_x >= front_buffer_->width() || destination_y >= front_buffer_->height())
    {
        return;
    }

    if (destination_x + source.width > front_buffer_->width())
    {
        source.width = front_buffer_->width() - destination_x;
    }
    if (destination_y + source.height > front_buffer_->height())
    {
        source.height = front_buffer_->height() - destination_y;
    }
    if (source.empty())
    {
        return;
    }

    const bool copy_backwards =
        destination_y > source.y || (destination_y == source.y && destination_x > source.x);
    if (copy_backwards)
    {
        for (uint64_t row = source.height; row > 0; --row)
        {
            const uint64_t current_row = row - 1;
            for (uint64_t column = source.width; column > 0; --column)
            {
                const uint64_t current_column = column - 1;
                front_buffer_->put_pixel(destination_x + current_column,
                                         destination_y + current_row,
                                         front_buffer_->pixel(source.x + current_column,
                                                              source.y + current_row));
            }
        }
        return;
    }

    for (uint64_t row = 0; row < source.height; ++row)
    {
        for (uint64_t column = 0; column < source.width; ++column)
        {
            front_buffer_->put_pixel(destination_x + column,
                                     destination_y + row,
                                     front_buffer_->pixel(source.x + column, source.y + row));
        }
    }
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
    if (!cursor.empty() && !present_rect(cursor))
    {
        return false;
    }

    const Rect copied = scene_buffer_->copy_rect(source, destination_x, destination_y);
    if (copied.empty())
    {
        return false;
    }
    copy_front_rect(source, destination_x, destination_y);
    return present_rect(cursor) && present_rect(copied);
}

} // namespace kernel::display
