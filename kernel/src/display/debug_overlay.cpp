#include "kernel/display/debug_overlay.hpp"

namespace
{

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

bool rects_overlap(kernel::display::Rect lhs, kernel::display::Rect rhs)
{
    return !kernel::display::intersect_rect(lhs, rhs).empty();
}

void append_char(char * buffer, size_t capacity, size_t & index, char value)
{
    if (capacity == 0 || index + 1 >= capacity)
    {
        return;
    }

    buffer[index++] = value;
    buffer[index] = '\0';
}

void append_string(char * buffer, size_t capacity, size_t & index, const char * value)
{
    while (*value != '\0')
    {
        append_char(buffer, capacity, index, *value);
        ++value;
    }
}

void append_decimal(char * buffer, size_t capacity, size_t & index, uint64_t value)
{
    char digits[20] = {};
    size_t digit_count = 0;

    if (value == 0)
    {
        append_char(buffer, capacity, index, '0');
        return;
    }

    while (value > 0 && digit_count < sizeof(digits))
    {
        digits[digit_count++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    while (digit_count > 0)
    {
        append_char(buffer, capacity, index, digits[--digit_count]);
    }
}

const char * mode_name(kernel::input::DeviceMode mode)
{
    switch (mode)
    {
    case kernel::input::DeviceMode::Irq:
        return "irq";
    case kernel::input::DeviceMode::PollingFallback:
        return "poll";
    }
    return "?";
}

void clear_lines(kernel::display::debug_overlay::Lines & lines)
{
    for (size_t index = 0; index < kernel::display::debug_overlay::kLineCapacity; ++index)
    {
        lines.first[index] = '\0';
        lines.second[index] = '\0';
    }
}

} // namespace

namespace kernel::display::debug_overlay
{

Rect bounds_for(uint64_t surface_width, uint64_t surface_height, Config config)
{
    if (surface_width <= config.margin * 2 || surface_height <= config.margin * 2 ||
        config.width == 0 || config.height == 0)
    {
        return {};
    }

    const uint64_t available_width = surface_width - (config.margin * 2);
    const uint64_t available_height = surface_height - (config.margin * 2);
    const uint64_t width = min_u64(config.width, available_width);
    const uint64_t height = min_u64(config.height, available_height);

    if (width == 0 || height == 0)
    {
        return {};
    }

    return {surface_width - config.margin - width, config.margin, width, height};
}

Rect bounds_for(uint64_t surface_width, uint64_t surface_height, Rect avoid_bounds, Config config)
{
    const Rect preferred = bounds_for(surface_width, surface_height, config);
    if (preferred.empty() || avoid_bounds.empty() || !rects_overlap(preferred, avoid_bounds))
    {
        return preferred;
    }

    const uint64_t bottom_limit =
        surface_height > config.margin ? surface_height - config.margin : 0;
    const uint64_t below_y = avoid_bounds.y + avoid_bounds.height + config.margin;
    if (below_y < bottom_limit && preferred.height <= bottom_limit - below_y)
    {
        return {preferred.x, below_y, preferred.width, preferred.height};
    }

    if (avoid_bounds.x > config.margin + preferred.width)
    {
        return {avoid_bounds.x - config.margin - preferred.width,
                preferred.y,
                preferred.width,
                preferred.height};
    }

    return preferred;
}

bool should_refresh(uint64_t last_ticks, uint64_t current_ticks, uint64_t refresh_interval_ticks)
{
    if (refresh_interval_ticks == 0 || current_ticks < last_ticks)
    {
        return true;
    }

    return current_ticks - last_ticks >= refresh_interval_ticks;
}

void format_snapshot(const Snapshot & snapshot, Lines & lines)
{
    clear_lines(lines);

    size_t first_index = 0;
    append_string(lines.first, kLineCapacity, first_index, "t:");
    append_decimal(lines.first, kLineCapacity, first_index, snapshot.ticks);
    append_string(lines.first, kLineCapacity, first_index, " q:");
    append_decimal(lines.first, kLineCapacity, first_index, snapshot.queued_events);
    append_string(lines.first, kLineCapacity, first_index, " d:");
    append_decimal(lines.first, kLineCapacity, first_index, snapshot.dropped_events);

    size_t second_index = 0;
    append_string(lines.second, kLineCapacity, second_index, "k:");
    append_string(lines.second, kLineCapacity, second_index, mode_name(snapshot.keyboard_mode));
    append_string(lines.second, kLineCapacity, second_index, " m:");
    append_string(lines.second, kLineCapacity, second_index, mode_name(snapshot.mouse_mode));
    append_string(lines.second, kLineCapacity, second_index, " f:");
    append_decimal(lines.second, kLineCapacity, second_index, snapshot.remaining_frames);
}

} // namespace kernel::display::debug_overlay
