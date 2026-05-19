#include "kernel/display/display_stats.hpp"

namespace kernel::display
{

namespace
{

uint64_t saturating_subtract(uint64_t lhs, uint64_t rhs)
{
    return lhs >= rhs ? lhs - rhs : 0;
}

DisplayFrameStats delta(DisplayFrameStats before, DisplayFrameStats after)
{
    return {
        saturating_subtract(after.frame_flush_count, before.frame_flush_count),
        saturating_subtract(after.present_operation_count, before.present_operation_count),
        saturating_subtract(after.present_rect_count, before.present_rect_count),
        saturating_subtract(after.present_scroll_count, before.present_scroll_count),
        saturating_subtract(after.total_presented_pixels, before.total_presented_pixels),
        saturating_subtract(after.largest_present_rect_area, before.largest_present_rect_area),
        saturating_subtract(after.large_present_fallback_count,
                            before.large_present_fallback_count),
    };
}

FramebufferPresenterStats delta(FramebufferPresenterStats before,
                                FramebufferPresenterStats after)
{
    return {
        saturating_subtract(after.present_call_count, before.present_call_count),
        saturating_subtract(after.present_rect_count, before.present_rect_count),
        saturating_subtract(after.present_scroll_count, before.present_scroll_count),
        saturating_subtract(after.total_presented_pixels, before.total_presented_pixels),
        saturating_subtract(after.largest_present_rect_area, before.largest_present_rect_area),
        saturating_subtract(after.fast_path_copy_pixels, before.fast_path_copy_pixels),
        saturating_subtract(after.front_scroll_copy_pixels, before.front_scroll_copy_pixels),
        saturating_subtract(after.overlay_blend_pixels, before.overlay_blend_pixels),
    };
}

CompositorRuntimeStats delta(CompositorRuntimeStats before, CompositorRuntimeStats after)
{
    return {
        saturating_subtract(after.scene_compose_pixels, before.scene_compose_pixels),
        saturating_subtract(after.scene_compose_from_backing_pixels,
                            before.scene_compose_from_backing_pixels),
        saturating_subtract(after.scene_preflight_pixels, before.scene_preflight_pixels),
        saturating_subtract(after.scene_scroll_copy_pixels, before.scene_scroll_copy_pixels),
        saturating_subtract(after.scene_scroll_count, before.scene_scroll_count),
        saturating_subtract(after.repaint_plan_count, before.repaint_plan_count),
        saturating_subtract(after.repaint_plan_fallback_count,
                            before.repaint_plan_fallback_count),
    };
}

DisplayRuntimeStats delta(DisplayRuntimeStats before, DisplayRuntimeStats after)
{
    return {
        saturating_subtract(after.terminal_backing_copy_pixels,
                            before.terminal_backing_copy_pixels),
        saturating_subtract(after.window_repaint_request_count,
                            before.window_repaint_request_count),
        saturating_subtract(after.window_repaint_pixels, before.window_repaint_pixels),
        saturating_subtract(after.window_largest_repaint_area,
                            before.window_largest_repaint_area),
        saturating_subtract(after.window_move_repaint_pixels,
                            before.window_move_repaint_pixels),
        saturating_subtract(after.window_visual_repaint_pixels,
                            before.window_visual_repaint_pixels),
    };
}

} // namespace

DisplayStatsSnapshot make_display_stats_snapshot(DisplayPipelineStats stats)
{
    return {stats};
}

DisplayPipelineStats display_stats_delta(DisplayStatsSnapshot before,
                                         DisplayStatsSnapshot after)
{
    return {
        delta(before.pipeline.frame, after.pipeline.frame),
        delta(before.pipeline.presenter, after.pipeline.presenter),
        delta(before.pipeline.compositor, after.pipeline.compositor),
        delta(before.pipeline.runtime, after.pipeline.runtime),
        saturating_subtract(after.pipeline.elapsed_ticks, before.pipeline.elapsed_ticks),
    };
}

} // namespace kernel::display
