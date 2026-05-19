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
inline constexpr uint64_t kItemMargin = 6;
inline constexpr uint64_t kDefaultItemWidth = 96;
inline constexpr uint64_t kItemMinWidth = 24;
inline constexpr uint64_t kItemMinHeight = 12;

enum class ItemKind
{
    None,
    Terminal,
};

enum class DesktopShellAction
{
    None,
    TerminalShowFocus,
};

enum class HitRegion
{
    None,
    Background,
    Item,
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

struct TerminalItemState
{
    bool app_visible = true;
    bool app_focused = true;
    bool app_active = true;
    bool app_closed = false;
};

struct Item
{
    Rect bounds;
    ItemKind kind = ItemKind::None;
    DesktopShellAction action = DesktopShellAction::None;
    bool visible = false;
    bool enabled = false;
    bool active = false;
    bool focused = false;

    [[nodiscard]] bool valid() const;
};

struct HitTestResult
{
    HitRegion region = HitRegion::None;
    ItemKind item_kind = ItemKind::None;
    DesktopShellAction action = DesktopShellAction::None;
    bool item_enabled = false;

    [[nodiscard]] bool hit_item() const;
};

[[nodiscard]] Config default_config();
[[nodiscard]] Rect bounds_for(Rect desktop_bounds, Config config = {});
[[nodiscard]] Rect work_area_for(Rect desktop_bounds, Rect bar_bounds, bool bar_visible);
[[nodiscard]] Layout layout_for(Rect desktop_bounds, Config config = {});
[[nodiscard]] GuiSurface make_surface(Rect desktop_bounds, GuiSurfaceId id = kGuiSurfaceId, Config config = {});
[[nodiscard]] bool should_redraw(const GuiSurface & surface);
[[nodiscard]] Item terminal_item_for(const GuiSurface & surface,
                                     Config config,
                                     TerminalItemState terminal);
[[nodiscard]] HitTestResult hit_test(const GuiSurface & surface,
                                     Config config,
                                     TerminalItemState terminal,
                                     uint64_t x,
                                     uint64_t y);
[[nodiscard]] PixelSample sample_pixel(const GuiSurface & surface,
                                       Palette palette,
                                       Config config,
                                       TerminalItemState terminal,
                                       uint64_t x,
                                       uint64_t y);

} // namespace kernel::display::desktop_bar
