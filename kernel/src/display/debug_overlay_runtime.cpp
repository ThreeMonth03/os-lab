#include "kernel/display/debug_overlay.hpp"

#include "kernel/display/compositor.hpp"
#include "kernel/display/mouse_cursor.hpp"
#include "kernel/input/input.hpp"
#include "kernel/memory/memory.hpp"
#include "kernel/text/font5x7.hpp"
#include "kernel/time/timer.hpp"

namespace
{

namespace debug_overlay = kernel::display::debug_overlay;
constexpr uint64_t kPadding = 2;
constexpr uint64_t kGlyphSpacing = 1;
constexpr uint64_t kLineHeight = kernel::Glyph5x7::height + 2;

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

void draw_glyph(char value, uint64_t x, uint64_t y)
{
    if (g_state.surface == nullptr || !g_state.surface->ready())
    {
        return;
    }

    const kernel::Glyph5x7 & glyph = kernel::Font5x7::glyph_for(value);
    for (uint64_t glyph_row = 0; glyph_row < kernel::Glyph5x7::height; ++glyph_row)
    {
        for (uint64_t glyph_column = 0; glyph_column < kernel::Glyph5x7::width; ++glyph_column)
        {
            const uint8_t mask = static_cast<uint8_t>(1u << (kernel::Glyph5x7::width - glyph_column - 1));
            if ((glyph.rows[glyph_row] & mask) != 0)
            {
                g_state.surface->put_pixel(x + glyph_column, y + glyph_row, g_state.foreground);
            }
        }
    }
}

void draw_line(const char * line, uint64_t x, uint64_t y)
{
    if (g_state.surface == nullptr || !g_state.surface->ready())
    {
        return;
    }

    const uint64_t right = g_state.target.bounds.x + g_state.target.bounds.width;
    while (*line != '\0' && x + kernel::Glyph5x7::width <= right)
    {
        draw_glyph(*line, x, y);
        x += kernel::Glyph5x7::width + kGlyphSpacing;
        ++line;
    }
}

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
    g_state.initialized = true;
    return true;
}

bool ready() { return g_state.initialized && g_state.surface != nullptr && g_state.surface->ready(); }

void refresh_if_due()
{
    if (!ready())
    {
        return;
    }

    const uint64_t current_ticks = time::timer::ticks();
    if (!should_refresh(g_state.last_refresh_ticks, current_ticks, g_state.config.refresh_interval_ticks))
    {
        return;
    }

    refresh_now(make_snapshot());
    g_state.last_refresh_ticks = current_ticks;
}

void refresh_now(const Snapshot & snapshot)
{
    if (!ready())
    {
        return;
    }

    Lines lines;
    format_snapshot(snapshot, lines);

    compositor::RedrawGuard redraw(g_state.target.bounds);
    g_state.surface->fill_rect(g_state.target.bounds, g_state.background);

    const uint64_t text_x = g_state.target.bounds.x + kPadding;
    uint64_t text_y = g_state.target.bounds.y + kPadding;
    const uint64_t bottom = g_state.target.bounds.y + g_state.target.bounds.height;
    if (text_y + Glyph5x7::height <= bottom)
    {
        draw_line(lines.first, text_x, text_y);
    }

    text_y += kLineHeight;
    if (text_y + Glyph5x7::height <= bottom)
    {
        draw_line(lines.second, text_x, text_y);
    }
}

} // namespace kernel::display::debug_overlay
