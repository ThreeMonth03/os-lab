#pragma once

#include "kernel/display/gui_panel.hpp"

namespace kernel::display::gui_panel
{

[[nodiscard]] bool init(Surface & surface,
                        const GuiSurface & panel,
                        Color border,
                        Color background,
                        Color foreground,
                        Config config = {});
void refresh_now();

} // namespace kernel::display::gui_panel
