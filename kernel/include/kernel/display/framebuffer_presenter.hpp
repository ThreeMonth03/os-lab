#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/scene_buffer.hpp"

namespace kernel::display
{

using CursorPixelReader = PixelSample (*)(uint64_t x, uint64_t y);
using CursorBoundsReader = Rect (*)();

inline constexpr size_t kMaxPresenterOverlays = 2;

class FramebufferPresenter
{
public:
    FramebufferPresenter() = default;

    void reset(Surface & front_buffer, SceneBuffer & scene_buffer);
    void set_cursor_overlay(CursorPixelReader pixel_reader, CursorBoundsReader bounds_reader);
    void set_overlay(size_t index, CursorPixelReader pixel_reader, CursorBoundsReader bounds_reader);

    bool ready() const { return front_buffer_ != nullptr && front_buffer_->ready() && scene_buffer_ != nullptr && scene_buffer_->ready(); }
    [[nodiscard]] bool present_rect(Rect rect);

private:
    [[nodiscard]] Rect overlay_bounds(size_t index) const;
    [[nodiscard]] bool present_overlay_rect(Rect rect);
    void put_presented_pixel(uint64_t x, uint64_t y);
    [[nodiscard]] bool copy_scene_to_front(Rect rect);

    Surface * front_buffer_ = nullptr;
    SceneBuffer * scene_buffer_ = nullptr;
    CursorPixelReader overlay_pixels_[kMaxPresenterOverlays] = {};
    CursorBoundsReader overlay_bounds_[kMaxPresenterOverlays] = {};
}; // end class FramebufferPresenter

} // namespace kernel::display
