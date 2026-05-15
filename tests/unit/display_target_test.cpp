#include <gtest/gtest.h>

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
    EXPECT_TRUE(descriptor(kernel::display::kConsoleSurfaceId,
                           kernel::display::DisplayTargetKind::Console,
                           {0, 0, 640, 480})
                    .valid());

    EXPECT_FALSE(descriptor(kernel::display::kInvalidSurfaceId,
                            kernel::display::DisplayTargetKind::Console,
                            {0, 0, 640, 480})
                     .valid());
    EXPECT_FALSE(descriptor(2, kernel::display::DisplayTargetKind::None, {0, 0, 640, 480}).valid());
    EXPECT_FALSE(descriptor(2, kernel::display::DisplayTargetKind::GuiSurface, {0, 0, 0, 480}).valid());
}

TEST(DisplayTargetTest, DefaultsActiveAndFocusedTargetToConsole)
{
    const kernel::display::DisplayTargetRegistry registry;

    EXPECT_EQ(registry.active_target_id(), kernel::display::kConsoleSurfaceId);
    EXPECT_EQ(registry.focused_target_id(), kernel::display::kConsoleSurfaceId);
}

TEST(DisplayTargetTest, RegistersAndFindsTargets)
{
    kernel::display::DisplayTargetRegistry registry;

    ASSERT_TRUE(registry.register_target(descriptor(kernel::display::kConsoleSurfaceId,
                                                    kernel::display::DisplayTargetKind::Console,
                                                    {0, 0, 800, 600})));

    const kernel::display::SurfaceDescriptor * target = registry.find(kernel::display::kConsoleSurfaceId);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->kind, kernel::display::DisplayTargetKind::Console);
    EXPECT_TRUE(target->active);
    EXPECT_TRUE(target->focused);
    EXPECT_EQ(registry.size(), 1u);
}

TEST(DisplayTargetTest, RejectsInvalidDuplicateAndFullTargetRegistrations)
{
    kernel::display::DisplayTargetRegistry registry;

    EXPECT_FALSE(registry.register_target(descriptor(kernel::display::kInvalidSurfaceId,
                                                     kernel::display::DisplayTargetKind::Console,
                                                     {0, 0, 800, 600})));

    ASSERT_TRUE(registry.register_target(descriptor(kernel::display::kConsoleSurfaceId,
                                                    kernel::display::DisplayTargetKind::Console,
                                                    {0, 0, 800, 600})));
    EXPECT_FALSE(registry.register_target(descriptor(kernel::display::kConsoleSurfaceId,
                                                     kernel::display::DisplayTargetKind::Console,
                                                     {0, 0, 800, 600})));

    for (kernel::display::SurfaceId id = 2; id <= kernel::display::kMaxDisplayTargets; ++id)
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

    ASSERT_TRUE(registry.register_target(descriptor(kernel::display::kConsoleSurfaceId,
                                                    kernel::display::DisplayTargetKind::Console,
                                                    {0, 0, 800, 600})));

    EXPECT_FALSE(registry.set_active(42));
    EXPECT_EQ(registry.active_target_id(), kernel::display::kConsoleSurfaceId);
}

TEST(DisplayTargetTest, SwitchesActiveAndFocusedTargets)
{
    kernel::display::DisplayTargetRegistry registry;

    ASSERT_TRUE(registry.register_target(descriptor(kernel::display::kConsoleSurfaceId,
                                                    kernel::display::DisplayTargetKind::Console,
                                                    {0, 0, 800, 600})));
    ASSERT_TRUE(registry.register_target(
        descriptor(2, kernel::display::DisplayTargetKind::DebugOverlay, {10, 20, 100, 50})));

    EXPECT_TRUE(registry.set_active(2));
    EXPECT_TRUE(registry.set_focused(2));

    const kernel::display::SurfaceDescriptor active = registry.active_target();
    const kernel::display::SurfaceDescriptor * console = registry.find(kernel::display::kConsoleSurfaceId);
    ASSERT_NE(console, nullptr);
    EXPECT_EQ(active.id, 2);
    EXPECT_TRUE(active.active);
    EXPECT_TRUE(active.focused);
    EXPECT_FALSE(console->active);
    EXPECT_FALSE(console->focused);
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
