#pragma once

#include <stdint.h>

namespace kernel::display
{

struct RgbColor
{
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    bool operator==(const RgbColor & other) const = default;
};

struct DisplayPalette
{
    RgbColor terminal_foreground;
    RgbColor terminal_background;
    RgbColor desktop_background;
    RgbColor panel_border;
    RgbColor panel_foreground;
    RgbColor debug_overlay_foreground;
    RgbColor debug_overlay_background;
};

constexpr DisplayPalette default_display_palette()
{
    return {
        {0xf5, 0xf5, 0xf5},
        {0x10, 0x14, 0x1c},
        {0x12, 0x16, 0x1e},
        {0x2e, 0x3a, 0x46},
        {0xd7, 0xdd, 0xe7},
        {0x92, 0xf7, 0xb8},
        {0x10, 0x14, 0x1c},
    };
}

} // namespace kernel::display
