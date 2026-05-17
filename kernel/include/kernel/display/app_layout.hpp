#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

struct TerminalAppLayoutConfig
{
    Rect desktop_bounds;
    Rect panel_bounds;
    bool panel_visible = false;
    uint64_t panel_margin = 0;
    uint64_t hidden_panel_top_safe_inset = 0;
    uint64_t cell_width = 1;
    uint64_t cell_height = 1;
}; // end struct TerminalAppLayoutConfig

class TerminalAppLayout
{
public:
    static Rect bounds_for(TerminalAppLayoutConfig config);
};

} // namespace kernel::display
