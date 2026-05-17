#include "kernel/debug/display_profile.hpp"

#include <stdint.h>

#include "kernel/drivers/serial.hpp"

namespace
{

namespace serial = kernel::drivers::serial;

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
    write_stat("frame flushes", delta.frame.frame_flush_count);
    write_stat("present rects", delta.frame.present_rect_count);
    write_stat("presented pixels", delta.frame.total_presented_pixels);
    write_stat("largest present rect area", delta.frame.largest_present_rect_area);
    write_stat("large fallback count", delta.frame.large_present_fallback_count);
    write_stat("scene compose pixels", delta.compositor.scene_compose_pixels);
    write_stat("scene preflight pixels", delta.compositor.scene_preflight_pixels);
    write_stat("scene scroll copy pixels", delta.compositor.scene_scroll_copy_pixels);
    write_stat("presenter fast-copy pixels", delta.presenter.fast_path_copy_pixels);
    write_stat("overlay blend pixels", delta.presenter.overlay_blend_pixels);
}

} // namespace kernel::debug
