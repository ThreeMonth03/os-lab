#include "debug_overlay_runtime.hpp"

#include "kernel/display/compositor.hpp"
#include "kernel/display/debug_overlay_renderer.hpp"
#include "kernel/input/input.hpp"
#include "kernel/memory/memory.hpp"
#include "kernel/time/timer.hpp"

namespace
{

namespace debug_overlay = kernel::display::debug_overlay;

struct OverlayState
{
    kernel::display::Surface * surface = nullptr;
    kernel::display::SurfaceDescriptor target;
    kernel::display::Color foreground;
    kernel::display::Color background;
    debug_overlay::Config config;
    uint64_t last_refresh_ticks = 0;
    bool initialized = false;
};

OverlayState g_state;

debug_overlay::Snapshot make_snapshot()
{
    debug_overlay::Snapshot snapshot;
    snapshot.ticks = kernel::time::timer::ticks();

    const kernel::input::Stats input_stats = kernel::input::stats();
    snapshot.queued_events = input_stats.queued_events;
    snapshot.dropped_events = input_stats.dropped_events;
    snapshot.keyboard_mode = input_stats.keyboard_mode;
    snapshot.mouse_mode = input_stats.mouse_mode;

    const kernel::memory::Stats memory_stats = kernel::memory::stats();
    snapshot.remaining_frames = memory_stats.frames.remaining_frames;
    return snapshot;
}

void paint_overlay_region(const debug_overlay::Snapshot & snapshot, kernel::display::Rect dirty_rect)
{
    debug_overlay::Lines lines;
    debug_overlay::format_snapshot(snapshot, lines);
    debug_overlay::paint_region(*g_state.surface,
                                g_state.target.bounds,
                                lines,
                                {g_state.foreground, g_state.background},
                                dirty_rect);
}

bool overlay_ready()
{
    return g_state.initialized && g_state.surface != nullptr && g_state.surface->ready();
}

void repaint_overlay(kernel::display::Rect dirty_rect)
{
    if (!overlay_ready() || debug_overlay::repaint_region(g_state.target.bounds, dirty_rect).empty())
    {
        return;
    }

    paint_overlay_region(make_snapshot(), dirty_rect);
}

void refresh_overlay_now(const debug_overlay::Snapshot & snapshot)
{
    if (!overlay_ready())
    {
        return;
    }

    paint_overlay_region(snapshot, g_state.target.bounds);
    kernel::display::compositor::repaint_layers_above(kernel::display::LayerKind::DebugOverlay,
                                                      g_state.target.bounds);
}

} // namespace

namespace kernel::display::debug_overlay
{

bool init(Surface & surface, const SurfaceDescriptor & target, Color foreground, Color background, Config config)
{
    g_state = {};
    if (!surface.ready() || !target.valid() || target.kind != DisplayTargetKind::DebugOverlay)
    {
        return false;
    }

    g_state.surface = &surface;
    g_state.target = target;
    g_state.foreground = foreground;
    g_state.background = background;
    g_state.config = config;
    if (!compositor::register_layer_repaint_callback(LayerKind::DebugOverlay, repaint_overlay))
    {
        g_state = {};
        return false;
    }

    g_state.initialized = true;
    return true;
}

void refresh_if_due()
{
    if (!overlay_ready())
    {
        return;
    }

    const uint64_t current_ticks = time::timer::ticks();
    if (!should_refresh(g_state.last_refresh_ticks, current_ticks, g_state.config.refresh_interval_ticks))
    {
        return;
    }

    refresh_overlay_now(make_snapshot());
    g_state.last_refresh_ticks = current_ticks;
}

} // namespace kernel::display::debug_overlay
