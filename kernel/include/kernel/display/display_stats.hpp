#pragma once

#include "kernel/display/compositor.hpp"
#include "kernel/display/display_frame.hpp"
#include "kernel/display/framebuffer_presenter.hpp"

namespace kernel::display
{

struct DisplayPipelineStats
{
    DisplayFrameStats frame;
    FramebufferPresenterStats presenter;
    CompositorRuntimeStats compositor;
};

struct DisplayStatsSnapshot
{
    DisplayPipelineStats pipeline;
};

[[nodiscard]] DisplayStatsSnapshot make_display_stats_snapshot(DisplayPipelineStats stats);
[[nodiscard]] DisplayPipelineStats display_stats_delta(DisplayStatsSnapshot before,
                                                       DisplayStatsSnapshot after);

} // namespace kernel::display
