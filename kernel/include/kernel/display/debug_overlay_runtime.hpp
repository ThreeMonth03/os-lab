#pragma once

#include "kernel/display/debug_overlay.hpp"

namespace kernel::display::debug_overlay
{

[[nodiscard]] bool init(Surface & surface,
                        const SurfaceDescriptor & target,
                        Color foreground,
                        Color background,
                        Config config = {});
void refresh_if_due();

} // namespace kernel::display::debug_overlay
