#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/scene_buffer.hpp"

namespace kernel::display
{

using CursorPixelReader = PixelSample (*)(uint64_t x, uint64_t y);
using CursorBoundsReader = Rect (*)();

class FramebufferPresenter
{
public:
    FramebufferPresenter() = default;

    void reset(Surface & front_buffer, SceneBuffer & scene_buffer);
    void set_cursor_overlay(CursorPixelReader pixel_reader, CursorBoundsReader bounds_reader);

    bool ready() const { return front_buffer_ != nullptr && front_buffer_->ready() && scene_buffer_ != nullptr && scene_buffer_->ready(); }
    [[nodiscard]] bool present_rect(Rect rect);
    [[nodiscard]] bool copy_scene_rect(Rect source, uint64_t destination_x, uint64_t destination_y);

private:
    [[nodiscard]] Rect cursor_bounds() const;
    void put_presented_pixel(uint64_t x, uint64_t y);
    void copy_front_rect(Rect source, uint64_t destination_x, uint64_t destination_y);

    Surface * front_buffer_ = nullptr;
    SceneBuffer * scene_buffer_ = nullptr;
    CursorPixelReader cursor_pixel_ = nullptr;
    CursorBoundsReader cursor_bounds_ = nullptr;
}; // end class FramebufferPresenter

} // namespace kernel::display
