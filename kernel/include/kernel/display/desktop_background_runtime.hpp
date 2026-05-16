#pragma once

#include "kernel/display/desktop_background.hpp"

namespace kernel::display::desktop_background
{

[[nodiscard]] bool init(Surface & surface, Rect bounds, SolidColorSource source);
void refresh_now();

} // namespace kernel::display::desktop_background
