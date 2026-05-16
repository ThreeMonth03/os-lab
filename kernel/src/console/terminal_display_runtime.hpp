#pragma once

#include <stdint.h>

#include "terminal_app.hpp"

#include "kernel/display/compositor.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/app_surface.hpp"
#include "kernel/display/gui_surface.hpp"
#include "kernel/display/hit_test.hpp"

namespace kernel::console
{

class TerminalDisplayRuntime
{
public:
    TerminalDisplayRuntime() = default;

    bool init(TerminalApp & terminal_app,
              display::compositor::LayerRepaintCallback terminal_repaint_callback);

    bool ready() const;
    display::HitTestResult pointer_target() const { return pointer_target_; }
    void update_pointer_target(uint64_t x, uint64_t y);

private:
    display::Surface surface_;
    display::DisplayTargetRegistry targets_;
    display::AppSurfaceRegistry app_surfaces_;
    display::GuiSurfaceRegistry gui_surfaces_;
    display::AppSurface terminal_app_surface_;
    display::HitTestResult pointer_target_;
};

TerminalDisplayRuntime & terminal_display_runtime();

} // namespace kernel::console
