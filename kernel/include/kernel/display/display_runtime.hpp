#pragma once

#include <stdint.h>

#include "kernel/display/display_stats.hpp"
#include "kernel/display/hit_test.hpp"
#include "kernel/display/pointer_cursor_shape.hpp"

namespace kernel::display::runtime
{

struct TerminalWindowInteractionResult
{
    bool handled = false;
    bool clear_keyboard_focus = false;
    bool focus_keyboard_terminal_app = false;
    bool app_resized = false;
    bool app_moved = false;
};

[[nodiscard]] HitTestResult pointer_target();
[[nodiscard]] PointerCursorShape pointer_cursor_shape();
[[nodiscard]] DisplayPipelineStats stats();
void reset_stats();

void refresh_debug_overlay_if_due();
void update_pointer_target(uint64_t x, uint64_t y);
TerminalWindowInteractionResult handle_terminal_window_pointer(uint64_t x,
                                                               uint64_t y,
                                                               bool primary_down);

} // namespace kernel::display::runtime
