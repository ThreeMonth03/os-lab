#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

struct DesktopAppLayoutConfig
{
    Rect desktop_bounds;
    Rect system_ui_bounds;
    bool system_ui_visible = false;
    uint64_t system_ui_margin = 0;
    uint64_t hidden_system_ui_top_safe_inset = 0;
    uint64_t minimum_cell_width = 1;
    uint64_t minimum_cell_height = 1;
}; // end struct DesktopAppLayoutConfig

struct AppCellCapacity
{
    uint64_t columns = 0;
    uint64_t rows = 0;

    bool valid() const { return columns > 0 && rows > 0; }
}; // end struct AppCellCapacity

class DesktopAppLayout
{
public:
    static Rect primary_app_bounds_for(DesktopAppLayoutConfig config);
    static AppCellCapacity cell_capacity_for(Rect bounds, uint64_t cell_width, uint64_t cell_height);
}; // end class DesktopAppLayout

} // namespace kernel::display
