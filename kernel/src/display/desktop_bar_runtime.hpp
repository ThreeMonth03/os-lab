#pragma once

#include "kernel/display/desktop_bar.hpp"

namespace kernel::display::desktop_bar
{

[[nodiscard]] bool init(const GuiSurface & bar, Palette palette, Config config);

} // namespace kernel::display::desktop_bar
