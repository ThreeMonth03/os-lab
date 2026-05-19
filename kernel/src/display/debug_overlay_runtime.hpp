#pragma once

#include "kernel/display/debug_overlay.hpp"

namespace kernel::display::debug_overlay
{

[[nodiscard]] bool init(const SurfaceDescriptor & target,
                        Color foreground,
                        Color background,
                        Config config = {});
[[nodiscard]] bool update_target(const SurfaceDescriptor & target);
[[nodiscard]] Rect bounds();
void refresh_if_due();

} // namespace kernel::display::debug_overlay
