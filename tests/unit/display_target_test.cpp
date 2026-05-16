#include <gtest/gtest.h>

#include "kernel/display/app_surface.hpp"
#include "kernel/display/display_target.hpp"

namespace
{

kernel::display::SurfaceDescriptor descriptor(kernel::display::SurfaceId id,
                                              kernel::display::DisplayTargetKind kind,
                                              kernel::display::Rect bounds)
{
    kernel::display::SurfaceDescriptor result;
    result.id = id;
    result.kind = kind;
    result.bounds = bounds;
    return result;
}

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

} // namespace

TEST(DisplayTargetTest, ValidatesSurfaceDescriptors)
{
    EXPECT_TRUE(descriptor(kernel::display::app_surface_display_id_for(
                               kernel::display::kTerminalAppSurfaceId),
                           kernel::display::DisplayTargetKind::AppSurface,
                           {0, 0, 640, 480})
                    .valid());

    EXPECT_FALSE(descriptor(kernel::display::kInvalidSurfaceId,
                            kernel::display::DisplayTargetKind::AppSurface,
                            {0, 0, 640, 480})
                     .valid());
    EXPECT_FALSE(descriptor(2, kernel::display::DisplayTargetKind::None, {0, 0, 640, 480}).valid());
    EXPECT_FALSE(descriptor(2, kernel::display::DisplayTargetKind::GuiSurface, {0, 0, 0, 480}).valid());
}

TEST(DisplayTargetTest, DefaultsActiveAndFocusedTargetToNone)
{
    const kernel::display::DisplayTargetRegistry registry;

    EXPECT_EQ(registry.active_target_id(), kernel::display::kInvalidSurfaceId);
    EXPECT_EQ(registry.focused_target_id(), kernel::display::kInvalidSurfaceId);
}

TEST(DisplayTargetTest, RegistersAndFindsTargets)
{
    kernel::display::DisplayTargetRegistry registry;
    constexpr kernel::display::SurfaceId kTargetId =
        kernel::display::app_surface_display_id_for(kernel::display::kTerminalAppSurfaceId);

    ASSERT_TRUE(registry.register_target(
        descriptor(kTargetId, kernel::display::DisplayTargetKind::AppSurface, {0, 0, 800, 600})));

    const kernel::display::SurfaceDescriptor * target = registry.find(kTargetId);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->kind, kernel::display::DisplayTargetKind::AppSurface);
    EXPECT_FALSE(target->active);
    EXPECT_FALSE(target->focused);
    EXPECT_EQ(registry.size(), 1u);
}

TEST(DisplayTargetTest, RejectsInvalidDuplicateAndFullTargetRegistrations)
{
    kernel::display::DisplayTargetRegistry registry;
    constexpr kernel::display::SurfaceId kFirstTargetId = 10;

    EXPECT_FALSE(registry.register_target(descriptor(kernel::display::kInvalidSurfaceId,
                                                     kernel::display::DisplayTargetKind::AppSurface,
                                                     {0, 0, 800, 600})));

    ASSERT_TRUE(registry.register_target(descriptor(kFirstTargetId,
                                                    kernel::display::DisplayTargetKind::AppSurface,
                                                    {0, 0, 800, 600})));
    EXPECT_FALSE(registry.register_target(descriptor(kFirstTargetId,
                                                     kernel::display::DisplayTargetKind::AppSurface,
                                                     {0, 0, 800, 600})));

    for (kernel::display::SurfaceId id = kFirstTargetId + 1;
         id < kFirstTargetId + kernel::display::kMaxDisplayTargets;
         ++id)
    {
        ASSERT_TRUE(registry.register_target(
            descriptor(id, kernel::display::DisplayTargetKind::GuiSurface, {0, 0, 10, 10})));
    }

    EXPECT_EQ(registry.size(), registry.capacity());
    EXPECT_FALSE(registry.register_target(
        descriptor(100, kernel::display::DisplayTargetKind::DebugOverlay, {0, 0, 10, 10})));
}

TEST(DisplayTargetTest, RejectsUnknownActiveTarget)
{
    kernel::display::DisplayTargetRegistry registry;
    constexpr kernel::display::SurfaceId kTargetId = 10;

    ASSERT_TRUE(registry.register_target(
        descriptor(kTargetId, kernel::display::DisplayTargetKind::AppSurface, {0, 0, 800, 600})));

    EXPECT_FALSE(registry.set_active(42));
    EXPECT_EQ(registry.active_target_id(), kernel::display::kInvalidSurfaceId);
}

TEST(DisplayTargetTest, SwitchesActiveAndFocusedTargets)
{
    kernel::display::DisplayTargetRegistry registry;
    constexpr kernel::display::SurfaceId kAppTargetId =
        kernel::display::app_surface_display_id_for(kernel::display::kTerminalAppSurfaceId);
    constexpr kernel::display::SurfaceId kOverlayTargetId = 2;

    ASSERT_TRUE(registry.register_target(descriptor(kAppTargetId,
                                                    kernel::display::DisplayTargetKind::AppSurface,
                                                    {0, 0, 800, 600})));
    ASSERT_TRUE(registry.register_target(
        descriptor(kOverlayTargetId,
                   kernel::display::DisplayTargetKind::DebugOverlay,
                   {10, 20, 100, 50})));

    EXPECT_TRUE(registry.set_active(kOverlayTargetId));
    EXPECT_TRUE(registry.set_focused(kOverlayTargetId));

    const kernel::display::SurfaceDescriptor active = registry.active_target();
    const kernel::display::SurfaceDescriptor * app = registry.find(kAppTargetId);
    ASSERT_NE(app, nullptr);
    EXPECT_EQ(active.id, kOverlayTargetId);
    EXPECT_TRUE(active.active);
    EXPECT_TRUE(active.focused);
    EXPECT_FALSE(app->active);
    EXPECT_FALSE(app->focused);
}

TEST(DisplayTargetTest, ClipsRectToTargetBounds)
{
    const kernel::display::SurfaceDescriptor target = descriptor(2,
                                                                 kernel::display::DisplayTargetKind::GuiSurface,
                                                                 {10, 20, 100, 50});

    expect_rect(kernel::display::clip_to_target(target, {0, 0, 30, 30}), 10, 20, 20, 10);
    expect_rect(kernel::display::clip_to_target(target, {50, 30, 20, 20}), 50, 30, 20, 20);
    expect_rect(kernel::display::clip_to_target(target, {200, 200, 10, 10}), 0, 0, 0, 0);
}
