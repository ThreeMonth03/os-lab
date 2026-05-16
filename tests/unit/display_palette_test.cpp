#include <gtest/gtest.h>

#include "kernel/display/display_palette.hpp"

TEST(DisplayPaletteTest, UsesLowContrastPanelBorder)
{
    constexpr kernel::display::DisplayPalette palette = kernel::display::default_display_palette();

    EXPECT_EQ(palette.panel_border, (kernel::display::RgbColor{0x2e, 0x3a, 0x46}));
    EXPECT_NE(palette.panel_border, (kernel::display::RgbColor{0x6b, 0xd6, 0xff}));
}

TEST(DisplayPaletteTest, SharesTerminalAndDebugOverlayBackground)
{
    constexpr kernel::display::DisplayPalette palette = kernel::display::default_display_palette();

    EXPECT_EQ(palette.debug_overlay_background, palette.terminal_background);
}
