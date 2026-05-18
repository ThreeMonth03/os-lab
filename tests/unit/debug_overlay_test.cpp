#include <gtest/gtest.h>

#include "kernel/display/app_surface.hpp"
#include "kernel/display/backing_surface.hpp"
#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/debug_overlay_renderer.hpp"

namespace
{

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

template <size_t Size>
void fill_pixels(uint32_t (&pixels)[Size], uint32_t value)
{
    for (uint32_t & pixel : pixels)
    {
        pixel = value;
    }
}

} // namespace

TEST(DebugOverlayTest, PlacesBoundsInUpperRightCorner)
{
    const kernel::display::debug_overlay::Config config{200, 20, 4, 25};

    expect_rect(kernel::display::debug_overlay::bounds_for(800, 600, config), 596, 4, 200, 20);
}

TEST(DebugOverlayTest, ClampsBoundsToSmallSurface)
{
    const kernel::display::debug_overlay::Config config{200, 20, 4, 25};

    expect_rect(kernel::display::debug_overlay::bounds_for(80, 40, config), 4, 4, 72, 20);
    expect_rect(kernel::display::debug_overlay::bounds_for(4, 40, config), 0, 0, 0, 0);
}

TEST(DebugOverlayTest, MovesBelowAvoidBoundsWhenPreferredUpperRightOverlaps)
{
    const kernel::display::debug_overlay::Config config{200, 20, 4, 25};

    expect_rect(kernel::display::debug_overlay::bounds_for(800,
                                                           600,
                                                           {0, 16, 800, 20},
                                                           config),
                596,
                40,
                200,
                20);
}

TEST(DebugOverlayTest, KeepsPreferredBoundsWhenAvoidBoundsDoNotOverlap)
{
    const kernel::display::debug_overlay::Config config{200, 20, 4, 25};

    expect_rect(kernel::display::debug_overlay::bounds_for(800,
                                                           600,
                                                           {0, 100, 800, 20},
                                                           config),
                596,
                4,
                200,
                20);
}

TEST(DebugOverlayTest, RefreshesOnIntervalOrCounterWrap)
{
    EXPECT_FALSE(kernel::display::debug_overlay::should_refresh(100, 124, 25));
    EXPECT_TRUE(kernel::display::debug_overlay::should_refresh(100, 125, 25));
    EXPECT_TRUE(kernel::display::debug_overlay::should_refresh(100, 101, 0));
    EXPECT_TRUE(kernel::display::debug_overlay::should_refresh(100, 10, 25));
}

TEST(DebugOverlayTest, FormatsSnapshotIntoFixedLines)
{
    kernel::display::debug_overlay::Snapshot snapshot;
    snapshot.ticks = 1234;
    snapshot.queued_events = 2;
    snapshot.dropped_events = 1;
    snapshot.keyboard_mode = kernel::input::DeviceMode::Irq;
    snapshot.mouse_mode = kernel::input::DeviceMode::PollingFallback;
    snapshot.remaining_frames = 42;

    kernel::display::debug_overlay::Lines lines;
    kernel::display::debug_overlay::format_snapshot(snapshot, lines);

    EXPECT_STREQ(lines.first, "t:1234 q:2 d:1");
    EXPECT_STREQ(lines.second, "k:irq m:poll f:42");
}

TEST(DebugOverlayTest, RegistryCanTrackAppAndOverlayTargets)
{
    kernel::display::DisplayTargetRegistry registry;
    const kernel::display::AppSurface app =
        kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                          {0, 0, 800, 600},
                                          true,
                                          true);

    EXPECT_TRUE(registry.register_target(app.display_target()));
    EXPECT_TRUE(registry.set_active(app.display_surface_id));
    EXPECT_TRUE(registry.set_focused(app.display_surface_id));
    EXPECT_TRUE(registry.register_target({
        kernel::display::debug_overlay::kSurfaceId,
        kernel::display::DisplayTargetKind::DebugOverlay,
        kernel::display::debug_overlay::bounds_for(800, 600),
        false,
        false,
    }));

    const kernel::display::SurfaceDescriptor * overlay =
        registry.find(kernel::display::debug_overlay::kSurfaceId);
    ASSERT_NE(overlay, nullptr);
    EXPECT_EQ(registry.size(), 2u);
    EXPECT_EQ(registry.active_target_id(), app.display_surface_id);
    EXPECT_EQ(registry.focused_target_id(), app.display_surface_id);
    EXPECT_FALSE(overlay->active);
    EXPECT_FALSE(overlay->focused);
}

TEST(DebugOverlayTest, ClipsRepaintRegionToOverlayBounds)
{
    const kernel::display::Rect bounds{10, 4, 40, 16};

    expect_rect(kernel::display::debug_overlay::repaint_region(bounds, {12, 6, 4, 3}), 12, 6, 4, 3);
    expect_rect(kernel::display::debug_overlay::repaint_region(bounds, {48, 18, 8, 8}), 48, 18, 2, 2);
    EXPECT_TRUE(kernel::display::debug_overlay::repaint_region(bounds, {0, 0, 4, 4}).empty());
}

TEST(DebugOverlayTest, PixelColorAtReturnsFinalGlyphOrBackgroundColor)
{
    kernel::display::debug_overlay::Lines lines;
    lines.first[0] = 't';
    lines.first[1] = '\0';

    const kernel::display::Rect overlay{4, 2, 20, 12};
    const kernel::display::debug_overlay::Palette palette{{2}, {1}};

    EXPECT_EQ(kernel::display::debug_overlay::pixel_color_at(overlay, lines, palette, 7, 4).value, 2u);
    EXPECT_EQ(kernel::display::debug_overlay::pixel_color_at(overlay, lines, palette, 6, 4).value, 1u);
}

TEST(DebugOverlayTest, PaintRegionOnlyWritesDirtyIntersection)
{
    constexpr uint64_t width = 32;
    constexpr uint64_t height = 18;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    kernel::display::debug_overlay::Lines lines;
    lines.first[0] = 't';
    lines.first[1] = '\0';

    kernel::display::debug_overlay::paint_region(surface,
                                                 {4, 2, 20, 12},
                                                 lines,
                                                 {{2}, {1}},
                                                 {4, 2, 2, 2});

    EXPECT_EQ(surface.pixel(4, 2).value, 1u);
    EXPECT_EQ(surface.pixel(5, 3).value, 1u);
    EXPECT_EQ(surface.pixel(6, 4).value, 9u);
    EXPECT_EQ(surface.pixel(23, 13).value, 9u);
    EXPECT_EQ(surface.pixel(3, 2).value, 9u);
}

TEST(DebugOverlayTest, PaintRegionWritesFinalGlyphPixelsInsideDirtyRect)
{
    constexpr uint64_t width = 32;
    constexpr uint64_t height = 18;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    kernel::display::debug_overlay::Lines lines;
    lines.first[0] = 't';
    lines.first[1] = '\0';

    kernel::display::debug_overlay::paint_region(surface,
                                                 {4, 2, 20, 12},
                                                 lines,
                                                 {{2}, {1}},
                                                 {7, 4, 1, 1});

    EXPECT_EQ(surface.pixel(7, 4).value, 2u);
    EXPECT_EQ(surface.pixel(8, 4).value, 9u);
}

TEST(DebugOverlayTest, PaintRegionWritesFinalBackgroundWhenDirtyMissesGlyphPixels)
{
    constexpr uint64_t width = 32;
    constexpr uint64_t height = 18;
    uint32_t pixels[width * height] = {};
    fill_pixels(pixels, 9);

    kernel::display::Surface surface(pixels, width, height, width * sizeof(uint32_t));
    kernel::display::debug_overlay::Lines lines;
    lines.first[0] = 't';
    lines.first[1] = '\0';

    kernel::display::debug_overlay::paint_region(surface,
                                                 {4, 2, 20, 12},
                                                 lines,
                                                 {{2}, {1}},
                                                 {6, 4, 1, 1});

    EXPECT_EQ(surface.pixel(6, 4).value, 1u);
    EXPECT_EQ(surface.pixel(7, 4).value, 9u);
}

TEST(DebugOverlayTest, PaintRegionCanUpdateBackingSurfaceGlyphPixels)
{
    uint32_t pixels[20 * 12] = {};
    kernel::display::BackingSurface backing(pixels, {4, 2, 20, 12}, 20);
    kernel::display::debug_overlay::Lines lines;
    lines.first[0] = 't';
    lines.first[1] = '\0';

    kernel::display::debug_overlay::paint_region(backing,
                                                 backing.bounds(),
                                                 lines,
                                                 {{2}, {1}},
                                                 {7, 4, 1, 1});

    EXPECT_EQ(backing.pixel(7, 4).value, 2u);
    EXPECT_EQ(pixels[(2 * 20) + 3], 2u);
}

TEST(DebugOverlayTest, PaintRegionDoesNotUpdateBackingSurfaceWhenDirtyMissesOverlay)
{
    uint32_t pixels[20 * 12] = {};
    fill_pixels(pixels, 9);
    kernel::display::BackingSurface backing(pixels, {4, 2, 20, 12}, 20);
    kernel::display::debug_overlay::Lines lines;
    lines.first[0] = 't';
    lines.first[1] = '\0';

    kernel::display::debug_overlay::paint_region(backing,
                                                 backing.bounds(),
                                                 lines,
                                                 {{2}, {1}},
                                                 {0, 0, 1, 1});

    EXPECT_EQ(backing.pixel(7, 4).value, 9u);
}
