#pragma once

#include "kernel/display/desktop_bar.hpp"

namespace kernel::display::desktop_bar
{

[[nodiscard]] bool init(const GuiSurface & bar, Palette palette, Config config);
void sync_terminal_item_state(TerminalItemState terminal);
void sync_dummy_debug_item_state(WindowItemState dummy);
[[nodiscard]] HitTestResult hit_test(uint64_t x, uint64_t y);
[[nodiscard]] bool action_enabled(DesktopShellAction action);

} // namespace kernel::display::desktop_bar
