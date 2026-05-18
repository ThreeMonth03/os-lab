#include "kernel/display/window_chrome.hpp"

namespace
{

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

bool contains(kernel::display::Rect rect, uint64_t x, uint64_t y)
{
    return !rect.empty() && x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

kernel::display::Rect close_button_bounds(kernel::display::Rect title_bar,
                                          kernel::display::WindowFrameConfig config)
{
    if (title_bar.empty() || config.close_button_size == 0 ||
        title_bar.width <= config.chrome_margin * 2 ||
        title_bar.height <= config.chrome_margin * 2)
    {
        return {};
    }

    const uint64_t size = min_u64(config.close_button_size,
                                  min_u64(title_bar.width - (config.chrome_margin * 2),
                                          title_bar.height - (config.chrome_margin * 2)));
    if (size == 0)
    {
        return {};
    }

    return {
        title_bar.x + title_bar.width - config.chrome_margin - size,
        title_bar.y + ((title_bar.height - size) / 2),
        size,
        size,
    };
}

kernel::display::Rect resize_handle_bounds(kernel::display::Rect outer_bounds,
                                           kernel::display::WindowFrameConfig config)
{
    if (outer_bounds.empty() || config.resize_handle_size == 0)
    {
        return {};
    }

    const uint64_t size = min_u64(config.resize_handle_size,
                                  min_u64(outer_bounds.width, outer_bounds.height));
    return {
        outer_bounds.x + outer_bounds.width - size,
        outer_bounds.y + outer_bounds.height - size,
        size,
        size,
    };
}

} // namespace

namespace kernel::display
{

WindowFrameMetrics WindowChrome::metrics_for(Rect outer_bounds, WindowFrameConfig config)
{
    if (outer_bounds.empty())
    {
        return {};
    }

    if (!config.visible)
    {
        return {
            outer_bounds,
            {},
            outer_bounds,
            {},
            {},
            0,
            false,
        };
    }

    const uint64_t border = config.border_thickness;
    const uint64_t title_height = config.title_bar_height;
    if (border == 0 || title_height == 0 || outer_bounds.width <= border * 2 ||
        outer_bounds.height <= title_height + border)
    {
        return {outer_bounds, {}, {}, {}, {}, border, true};
    }

    const Rect title_bar = {
        outer_bounds.x,
        outer_bounds.y,
        outer_bounds.width,
        title_height,
    };
    const Rect client = {
        outer_bounds.x + border,
        outer_bounds.y + title_height,
        outer_bounds.width - (border * 2),
        outer_bounds.height - title_height - border,
    };

    return {
        outer_bounds,
        title_bar,
        client,
        close_button_bounds(title_bar, config),
        resize_handle_bounds(outer_bounds, config),
        border,
        true,
    };
}

WindowChromeHitRegion WindowChrome::hit_test(WindowFrameMetrics metrics, uint64_t x, uint64_t y)
{
    if (!metrics.valid() || !contains(metrics.outer_bounds, x, y))
    {
        return WindowChromeHitRegion::None;
    }
    if (!metrics.visible)
    {
        return WindowChromeHitRegion::Content;
    }
    if (contains(metrics.close_button_bounds, x, y))
    {
        return WindowChromeHitRegion::CloseButton;
    }
    if (contains(metrics.resize_handle_bounds, x, y))
    {
        return WindowChromeHitRegion::ResizeHandle;
    }
    if (contains(metrics.client_bounds, x, y))
    {
        return WindowChromeHitRegion::Content;
    }
    if (contains(metrics.title_bar_bounds, x, y))
    {
        return WindowChromeHitRegion::TitleBar;
    }
    return WindowChromeHitRegion::Border;
}

} // namespace kernel::display
