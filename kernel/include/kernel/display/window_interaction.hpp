#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/pointer_cursor_shape.hpp"
#include "kernel/display/window_chrome.hpp"

namespace kernel::display
{

enum class WindowInteractionMode
{
    None,
    Move,
    Resize,
    Close,
};

enum class WindowResizeEdge : uint8_t
{
    None = 0,
    Left = 1,
    Right = 2,
    Top = 4,
    Bottom = 8,
    TopLeft = 5,
    TopRight = 6,
    BottomLeft = 9,
    BottomRight = 10,
};

struct WindowResizeConstraints
{
    Rect work_area;
    uint64_t minimum_client_width = 1;
    uint64_t minimum_client_height = 1;
    WindowFrameConfig frame_config;
};

struct WindowPointerEvent
{
    Rect current_bounds;
    WindowChromeHitRegion hit_region = WindowChromeHitRegion::None;
    uint64_t x = 0;
    uint64_t y = 0;
    bool primary_down = false;
    WindowResizeConstraints constraints;
};

struct WindowInteractionResult
{
    bool handled = false;
    bool commit_move = false;
    bool commit_resize = false;
    bool close_requested = false;
    Rect proposed_bounds;
    WindowInteractionMode mode = WindowInteractionMode::None;
    PointerCursorShape cursor_shape = PointerCursorShape::Arrow;
};

WindowResizeEdge resize_edges_for_hit_region(WindowChromeHitRegion region);
PointerCursorShape cursor_shape_for_hit_region(WindowChromeHitRegion region);
bool resize_edge_contains(WindowResizeEdge edges, WindowResizeEdge edge);

class WindowMoveDrag
{
public:
    WindowMoveDrag() = default;

    static WindowMoveDrag begin(Rect start_bounds,
                                uint64_t anchor_x,
                                uint64_t anchor_y,
                                Rect work_area);

    bool valid() const;
    Rect bounds_for(uint64_t pointer_x, uint64_t pointer_y) const;

private:
    WindowMoveDrag(Rect start_bounds, uint64_t anchor_x, uint64_t anchor_y, Rect work_area);

    Rect start_bounds_;
    uint64_t anchor_x_ = 0;
    uint64_t anchor_y_ = 0;
    Rect work_area_;
};

class WindowResizeDrag
{
public:
    WindowResizeDrag() = default;

    static WindowResizeDrag begin(Rect start_bounds,
                                  uint64_t anchor_x,
                                  uint64_t anchor_y,
                                  WindowResizeEdge edges,
                                  WindowResizeConstraints constraints);

    bool valid() const;
    Rect bounds_for(uint64_t pointer_x, uint64_t pointer_y) const;

private:
    WindowResizeDrag(Rect start_bounds,
                     uint64_t anchor_x,
                     uint64_t anchor_y,
                     WindowResizeEdge edges,
                     WindowResizeConstraints constraints);

    Rect start_bounds_;
    uint64_t anchor_x_ = 0;
    uint64_t anchor_y_ = 0;
    WindowResizeEdge edges_ = WindowResizeEdge::None;
    WindowResizeConstraints constraints_;
};

class WindowInteractionController
{
public:
    WindowInteractionController() = default;

    WindowInteractionResult update(WindowPointerEvent event);
    void reset();

    WindowInteractionMode mode() const { return mode_; }
    bool active() const { return mode_ != WindowInteractionMode::None; }

private:
    WindowInteractionResult start_interaction(WindowPointerEvent event);
    WindowInteractionResult update_active(WindowPointerEvent event, bool released);

    bool previous_primary_down_ = false;
    WindowInteractionMode mode_ = WindowInteractionMode::None;
    PointerCursorShape active_cursor_shape_ = PointerCursorShape::Arrow;
    WindowMoveDrag move_drag_;
    WindowResizeDrag resize_drag_;
};

} // namespace kernel::display
