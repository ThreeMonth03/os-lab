#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/input/input_types.hpp"

namespace kernel::display::debug_overlay
{

inline constexpr SurfaceId kSurfaceId = 2;
inline constexpr uint64_t kDefaultWidth = 224;
inline constexpr uint64_t kDefaultHeight = 22;
inline constexpr uint64_t kDefaultMargin = 4;
inline constexpr uint64_t kDefaultRefreshIntervalTicks = 25;
inline constexpr size_t kLineCapacity = 48;

struct Config
{
    uint64_t width = kDefaultWidth;
    uint64_t height = kDefaultHeight;
    uint64_t margin = kDefaultMargin;
    uint64_t refresh_interval_ticks = kDefaultRefreshIntervalTicks;
};

struct Snapshot
{
    uint64_t ticks = 0;
    uint64_t queued_events = 0;
    uint64_t dropped_events = 0;
    uint64_t remaining_frames = 0;
    input::DeviceMode keyboard_mode = input::DeviceMode::PollingFallback;
    input::DeviceMode mouse_mode = input::DeviceMode::PollingFallback;
};

struct Lines
{
    char first[kLineCapacity] = {};
    char second[kLineCapacity] = {};
};

[[nodiscard]] Rect bounds_for(uint64_t surface_width, uint64_t surface_height, Config config = {});
[[nodiscard]] bool should_refresh(uint64_t last_ticks, uint64_t current_ticks, uint64_t refresh_interval_ticks);
void format_snapshot(const Snapshot & snapshot, Lines & lines);

[[nodiscard]] bool init(Surface & surface, const SurfaceDescriptor & target, Color foreground, Color background, Config config = {});
bool ready();
void refresh_if_due();
void refresh_now();
void refresh_now(const Snapshot & snapshot);

} // namespace kernel::display::debug_overlay
