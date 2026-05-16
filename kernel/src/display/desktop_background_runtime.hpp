#pragma once

#include "kernel/display/desktop_background.hpp"

namespace kernel::display::desktop_background
{

[[nodiscard]] bool init(Surface & surface, Rect bounds, BackgroundSource source);

} // namespace kernel::display::desktop_background
