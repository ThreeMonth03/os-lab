#include "kernel/display/window_interaction.hpp"

namespace
{

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

uint64_t outer_width_for_client(kernel::display::WindowResizeConstraints constraints)
{
    if (!constraints.frame_config.visible)
    {
        return constraints.minimum_client_width;
    }

    return constraints.minimum_client_width + (constraints.frame_config.border_thickness * 2);
}

uint64_t outer_height_for_client(kernel::display::WindowResizeConstraints constraints)
{
    if (!constraints.frame_config.visible)
    {
        return constraints.minimum_client_height;
    }

    return constraints.minimum_client_height + constraints.frame_config.title_bar_height +
           constraints.frame_config.border_thickness;
}

uint64_t resized_extent(uint64_t start_size, uint64_t anchor, uint64_t pointer)
{
    if (pointer >= anchor)
    {
        return start_size + (pointer - anchor);
    }

    const uint64_t shrink = anchor - pointer;
    return shrink >= start_size ? 0 : start_size - shrink;
}

} // namespace

namespace kernel::display
{

WindowResizeDrag WindowResizeDrag::begin(Rect start_bounds,
                                         uint64_t anchor_x,
                                         uint64_t anchor_y,
                                         WindowResizeConstraints constraints)
{
    return {start_bounds, anchor_x, anchor_y, constraints};
}

WindowResizeDrag::WindowResizeDrag(Rect start_bounds,
                                   uint64_t anchor_x,
                                   uint64_t anchor_y,
                                   WindowResizeConstraints constraints)
    : start_bounds_(start_bounds)
    , anchor_x_(anchor_x)
    , anchor_y_(anchor_y)
    , constraints_(constraints)
{
}

bool WindowResizeDrag::valid() const
{
    return !start_bounds_.empty() && !constraints_.work_area.empty() &&
           constraints_.minimum_client_width > 0 && constraints_.minimum_client_height > 0;
}

Rect WindowResizeDrag::bounds_for(uint64_t pointer_x, uint64_t pointer_y) const
{
    if (!valid())
    {
        return {};
    }

    const uint64_t min_width = outer_width_for_client(constraints_);
    const uint64_t min_height = outer_height_for_client(constraints_);
    if (min_width == 0 || min_height == 0)
    {
        return {};
    }

    const uint64_t work_right = constraints_.work_area.x + constraints_.work_area.width;
    const uint64_t work_bottom = constraints_.work_area.y + constraints_.work_area.height;
    if (start_bounds_.x >= work_right || start_bounds_.y >= work_bottom)
    {
        return {};
    }

    const uint64_t max_width = work_right - start_bounds_.x;
    const uint64_t max_height = work_bottom - start_bounds_.y;
    if (max_width < min_width || max_height < min_height)
    {
        return {};
    }

    const uint64_t requested_width = resized_extent(start_bounds_.width, anchor_x_, pointer_x);
    const uint64_t requested_height = resized_extent(start_bounds_.height, anchor_y_, pointer_y);

    return {
        start_bounds_.x,
        start_bounds_.y,
        min_u64(max_u64(requested_width, min_width), max_width),
        min_u64(max_u64(requested_height, min_height), max_height),
    };
}

} // namespace kernel::display
