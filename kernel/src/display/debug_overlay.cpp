#include "kernel/display/debug_overlay.hpp"

namespace
{

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

uint64_t rect_right(kernel::display::Rect rect)
{
    return rect.x + rect.width;
}

uint64_t rect_bottom(kernel::display::Rect rect)
{
    return rect.y + rect.height;
}

bool rects_overlap(kernel::display::Rect lhs, kernel::display::Rect rhs)
{
    return !kernel::display::intersect_rect(lhs, rhs).empty();
}

bool contains_rect(kernel::display::Rect bounds, kernel::display::Rect rect)
{
    return !bounds.empty() && !rect.empty() && rect.x >= bounds.x && rect.y >= bounds.y &&
           rect_right(rect) <= rect_right(bounds) && rect_bottom(rect) <= rect_bottom(bounds);
}

kernel::display::Rect status_bounds_in(kernel::display::Rect bounds,
                                       kernel::display::debug_overlay::Config config,
                                       bool align_top)
{
    if (bounds.empty() || bounds.width <= config.margin * 2 || config.width == 0 ||
        config.height == 0)
    {
        return {};
    }

    const uint64_t available_width = bounds.width - (config.margin * 2);
    const uint64_t available_height =
        align_top && bounds.height > config.margin * 2 ? bounds.height - (config.margin * 2)
                                                       : bounds.height;
    const uint64_t width = min_u64(config.width, available_width);
    const uint64_t height = min_u64(config.height, available_height);
    if (width == 0 || height == 0 || height > bounds.height)
    {
        return {};
    }

    const uint64_t y = align_top ? bounds.y + config.margin
                                 : bounds.y + ((bounds.height - height) / 2);
    if (align_top && (y < bounds.y || rect_bottom({bounds.x, y, width, height}) > rect_bottom(bounds)))
    {
        return {};
    }

    return {rect_right(bounds) - config.margin - width, y, width, height};
}

bool placement_conflicts(kernel::display::Rect candidate,
                         kernel::display::debug_overlay::DesktopStatusPlacement placement)
{
    return rects_overlap(candidate, placement.app_chrome_avoid_bounds) ||
           rects_overlap(candidate, placement.desktop_bar_item_bounds);
}

bool placement_allowed(kernel::display::Rect candidate,
                       kernel::display::debug_overlay::DesktopStatusPlacement placement)
{
    return contains_rect(placement.desktop_bounds, candidate) &&
           !placement_conflicts(candidate, placement);
}

kernel::display::Rect first_allowed(kernel::display::Rect candidate,
                                    kernel::display::debug_overlay::DesktopStatusPlacement placement)
{
    return placement_allowed(candidate, placement) ? candidate : kernel::display::Rect{};
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
    return status_bounds_in({0, 0, surface_width, surface_height}, config, true);
}

Rect bounds_for(uint64_t surface_width, uint64_t surface_height, Rect avoid_bounds, Config config)
{
    return desktop_status_bounds_for({
        {0, 0, surface_width, surface_height},
        avoid_bounds,
        {},
        {},
        config,
    });
}

Rect desktop_status_bounds_for(DesktopStatusPlacement placement)
{
    const Rect bar_candidate = status_bounds_in(placement.desktop_bar_bounds,
                                                placement.config,
                                                false);
    const Rect allowed_bar_candidate = first_allowed(bar_candidate, placement);
    if (!allowed_bar_candidate.empty())
    {
        return allowed_bar_candidate;
    }

    const Rect preferred = status_bounds_in(placement.desktop_bounds, placement.config, true);
    if (preferred.empty() || !placement_conflicts(preferred, placement))
    {
        return preferred;
    }

    const uint64_t bottom_limit =
        rect_bottom(placement.desktop_bounds) > placement.config.margin
            ? rect_bottom(placement.desktop_bounds) - placement.config.margin
            : 0;
    const uint64_t below_y =
        rect_bottom(placement.app_chrome_avoid_bounds) + placement.config.margin;
    const Rect below_candidate = {preferred.x, below_y, preferred.width, preferred.height};
    if (below_y < bottom_limit && preferred.height <= bottom_limit - below_y &&
        placement_allowed(below_candidate, placement))
    {
        return below_candidate;
    }

    if (placement.app_chrome_avoid_bounds.x > placement.config.margin + preferred.width)
    {
        const Rect left_candidate = {
            placement.app_chrome_avoid_bounds.x - placement.config.margin - preferred.width,
            preferred.y,
            preferred.width,
            preferred.height,
        };
        if (placement_allowed(left_candidate, placement))
        {
            return left_candidate;
        }
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
