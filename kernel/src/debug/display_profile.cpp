#include "kernel/debug/display_profile.hpp"

#include <stdint.h>

#include "kernel/display/display_profile.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/time/timer.hpp"

namespace
{

namespace serial = kernel::drivers::serial;

kernel::display::CommandProfileTracker g_command_profile;
uint64_t g_command_start_ticks = 0;

void write_stat(kernel::StringView name, uint64_t value)
{
    serial::write_string("  ");
    serial::write_string(name);
    serial::write_string(": ");
    serial::write_decimal(value);
    serial::write_string("\n");
}

} // namespace

namespace kernel::debug
{

void write_display_profile_delta(StringView command, display::DisplayPipelineStats delta)
{
    serial::write_string("os-lab display profile: command=");
    serial::write_line(command);
    write_stat("elapsed ticks", delta.elapsed_ticks);
    write_stat("frame flushes", delta.frame.frame_flush_count);
    write_stat("present operations", delta.frame.present_operation_count);
    write_stat("presenter present calls", delta.presenter.present_call_count);
    write_stat("present rects", delta.frame.present_rect_count);
    write_stat("present scrolls", delta.frame.present_scroll_count);
    write_stat("presenter present rects", delta.presenter.present_rect_count);
    write_stat("presenter present scrolls", delta.presenter.present_scroll_count);
    write_stat("presented pixels", delta.frame.total_presented_pixels);
    write_stat("largest present rect area", delta.frame.largest_present_rect_area);
    write_stat("presenter largest present rect area", delta.presenter.largest_present_rect_area);
    write_stat("large fallback count", delta.frame.large_present_fallback_count);
    write_stat("terminal backing copy pixels", delta.runtime.terminal_backing_copy_pixels);
    write_stat("window repaint requests", delta.runtime.window_repaint_request_count);
    write_stat("window repaint pixels", delta.runtime.window_repaint_pixels);
    write_stat("window largest repaint area", delta.runtime.window_largest_repaint_area);
    write_stat("window move repaint pixels", delta.runtime.window_move_repaint_pixels);
    write_stat("window visual repaint pixels", delta.runtime.window_visual_repaint_pixels);
    write_stat("window preview repaint pixels", delta.runtime.window_preview_repaint_pixels);
    write_stat("window interaction pointer events",
               delta.runtime.window_interaction_pointer_events);
    write_stat("scene compose pixels", delta.compositor.scene_compose_pixels);
    write_stat("scene compose from backing pixels", delta.compositor.scene_compose_from_backing_pixels);
    write_stat("scene preflight pixels", delta.compositor.scene_preflight_pixels);
    write_stat("scene scroll copy pixels", delta.compositor.scene_scroll_copy_pixels);
    write_stat("scene move copy pixels", delta.compositor.scene_move_copy_pixels);
    write_stat("scene scroll count", delta.compositor.scene_scroll_count);
    write_stat("repaint plan count", delta.compositor.repaint_plan_count);
    write_stat("repaint plan fallback count", delta.compositor.repaint_plan_fallback_count);
    write_stat("presenter fast-copy pixels", delta.presenter.fast_path_copy_pixels);
    write_stat("presenter front-scroll pixels", delta.presenter.front_scroll_copy_pixels);
    write_stat("overlay blend pixels", delta.presenter.overlay_blend_pixels);
}

void begin_display_profile_command(StringView command, display::DisplayPipelineStats before)
{
    g_command_start_ticks = time::timer::ticks();
    g_command_profile.begin(command, before);
}

void finish_display_profile_command(display::DisplayPipelineStats after)
{
    const display::CommandProfileDelta result = g_command_profile.finish(after);
    if (result.valid)
    {
        display::DisplayPipelineStats delta = result.delta;
        delta.elapsed_ticks = time::timer::ticks() - g_command_start_ticks;
        write_display_profile_delta(result.command, delta);
    }
}

} // namespace kernel::debug
