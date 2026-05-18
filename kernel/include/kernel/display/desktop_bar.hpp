#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/gui_surface.hpp"

namespace kernel::display::desktop_bar
{

inline constexpr GuiSurfaceId kGuiSurfaceId = 1;
inline constexpr uint64_t kDefaultHeight = 32;
inline constexpr uint64_t kMinimumHeight = 8;
inline constexpr uint64_t kTopBorderHeight = 1;
inline constexpr uint64_t kButtonMargin = 6;
inline constexpr uint64_t kTerminalButtonWidth = 96;
inline constexpr uint64_t kTerminalButtonMinWidth = 24;
inline constexpr uint64_t kTerminalButtonMinHeight = 12;

enum class ButtonKind
{
    None,
    Terminal,
};

enum class HitRegion
{
    None,
    Background,
    TerminalButton,
};

struct Config
{
    uint64_t height = kDefaultHeight;
    bool visible = false;
    bool debug_actions = false;
};

struct Palette
{
    Color border;
    Color background;
    Color button_border;
    Color button_background;
    Color button_disabled_background;
    Color button_icon;
};

struct Layout
{
    Rect desktop_bounds;
    Rect bar_bounds;
    Rect work_area;
    bool bar_visible = false;
};

struct TerminalButtonState
{
    bool app_visible = true;
    bool app_closed = false;
};

struct Button
{
    Rect bounds;
    ButtonKind kind = ButtonKind::None;
    bool visible = false;
    bool enabled = false;
    bool active = false;

    [[nodiscard]] bool valid() const;
};

[[nodiscard]] Config default_config();
[[nodiscard]] Rect bounds_for(Rect desktop_bounds, Config config = {});
[[nodiscard]] Rect work_area_for(Rect desktop_bounds, Rect bar_bounds, bool bar_visible);
[[nodiscard]] Layout layout_for(Rect desktop_bounds, Config config = {});
[[nodiscard]] GuiSurface make_surface(Rect desktop_bounds, GuiSurfaceId id = kGuiSurfaceId, Config config = {});
[[nodiscard]] bool should_redraw(const GuiSurface & surface);
[[nodiscard]] Button terminal_button_for(const GuiSurface & surface,
                                         Config config,
                                         TerminalButtonState terminal);
[[nodiscard]] HitRegion hit_test(const GuiSurface & surface,
                                 Config config,
                                 TerminalButtonState terminal,
                                 uint64_t x,
                                 uint64_t y);
[[nodiscard]] PixelSample sample_pixel(const GuiSurface & surface,
                                       Palette palette,
                                       Config config,
                                       TerminalButtonState terminal,
                                       uint64_t x,
                                       uint64_t y);

} // namespace kernel::display::desktop_bar
