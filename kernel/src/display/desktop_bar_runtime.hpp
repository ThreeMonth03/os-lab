#pragma once

#include "kernel/display/desktop_bar.hpp"

namespace kernel::display::desktop_bar
{

[[nodiscard]] bool init(const GuiSurface & bar, Palette palette, Config config);
void sync_terminal_button_state(TerminalButtonState terminal);
[[nodiscard]] HitRegion hit_test(uint64_t x, uint64_t y);
[[nodiscard]] bool terminal_button_enabled();

} // namespace kernel::display::desktop_bar
