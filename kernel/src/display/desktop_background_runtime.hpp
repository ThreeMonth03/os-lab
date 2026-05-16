#pragma once

#include "kernel/display/desktop_background.hpp"

namespace kernel::display::desktop_background
{

[[nodiscard]] bool init(Surface & surface, Rect bounds, SolidColorSource source);

} // namespace kernel::display::desktop_background
