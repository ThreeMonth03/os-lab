#pragma once

#include <stdint.h>

#include "kernel/display/compositor.hpp"
#include "kernel/display/display_frame.hpp"
#include "kernel/display/framebuffer_presenter.hpp"
#include "kernel/display/hit_test.hpp"

namespace kernel::display::runtime
{

struct DisplayPipelineStats
{
    DisplayFrameStats frame;
    FramebufferPresenterStats presenter;
    CompositorRuntimeStats compositor;
};

[[nodiscard]] HitTestResult pointer_target();
[[nodiscard]] DisplayPipelineStats stats();
void reset_stats();

void refresh_debug_overlay_if_due();
void update_pointer_target(uint64_t x, uint64_t y);

} // namespace kernel::display::runtime
