#pragma once

#include "kernel/display/compositor.hpp"
#include "kernel/display/display_frame.hpp"
#include "kernel/display/framebuffer_presenter.hpp"

namespace kernel::display
{

struct DisplayRuntimeStats
{
    uint64_t terminal_backing_copy_pixels = 0;
    uint64_t window_repaint_request_count = 0;
    uint64_t window_repaint_pixels = 0;
    uint64_t window_largest_repaint_area = 0;
    uint64_t window_move_repaint_pixels = 0;
    uint64_t window_visual_repaint_pixels = 0;
};

struct DisplayPipelineStats
{
    DisplayFrameStats frame;
    FramebufferPresenterStats presenter;
    CompositorRuntimeStats compositor;
    DisplayRuntimeStats runtime;
    uint64_t elapsed_ticks = 0;
};

struct DisplayStatsSnapshot
{
    DisplayPipelineStats pipeline;
};

[[nodiscard]] DisplayStatsSnapshot make_display_stats_snapshot(DisplayPipelineStats stats);
[[nodiscard]] DisplayPipelineStats display_stats_delta(DisplayStatsSnapshot before,
                                                       DisplayStatsSnapshot after);

} // namespace kernel::display
