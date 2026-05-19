#include <gtest/gtest.h>

#include "kernel/display/app_surface.hpp"

namespace
{

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

} // namespace

TEST(AppSurfaceTest, BuildsTerminalAppSurfaceDescriptorAndLayer)
{
    const kernel::display::AppSurface surface =
        kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                          {8, 16, 320, 200},
                                          true,
                                          true,
                                          true);

    ASSERT_TRUE(surface.valid());
    EXPECT_TRUE(surface.open());
    EXPECT_TRUE(surface.visible());
    EXPECT_TRUE(surface.active);
    EXPECT_EQ(surface.display_surface_id,
              kernel::display::app_surface_display_id_for(kernel::display::kTerminalAppSurfaceId));

    const kernel::display::SurfaceDescriptor target = surface.display_target();
    EXPECT_EQ(target.id, surface.display_surface_id);
    EXPECT_EQ(target.kind, kernel::display::DisplayTargetKind::AppSurface);
    EXPECT_TRUE(target.active);
    EXPECT_TRUE(target.focused);
    expect_rect(target.bounds, 8, 16, 320, 200);

    const kernel::display::Layer layer = surface.composited_surface().layer();
    EXPECT_EQ(layer.kind, kernel::display::LayerKind::AppSurface);
    EXPECT_TRUE(layer.visible);
    EXPECT_TRUE(layer.occludes_lower_repaint());
    expect_rect(layer.bounds, 8, 16, 320, 200);
}

TEST(AppSurfaceTest, RejectsInvalidSurface)
{
    EXPECT_FALSE(kernel::display::make_app_surface(kernel::display::kInvalidAppSurfaceId,
                                                   {0, 0, 320, 200})
                     .valid());
    EXPECT_FALSE(kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                                   {0, 0, 0, 200})
                     .valid());
}

TEST(AppSurfaceTest, RegistryRegistersFindsAndFocusesSurface)
{
    kernel::display::AppSurfaceRegistry registry;
    const kernel::display::AppSurface surface =
        kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                          {0, 0, 640, 480});

    ASSERT_TRUE(registry.register_surface(surface));
    EXPECT_EQ(registry.size(), 1u);

    const kernel::display::AppSurface * found =
        registry.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->display_surface_id, surface.display_surface_id);
    EXPECT_EQ(registry.find_by_display_surface(surface.display_surface_id), found);

    EXPECT_TRUE(registry.set_focused(kernel::display::kTerminalAppSurfaceId));
    const kernel::display::AppSurface * focused = registry.focused_surface();
    ASSERT_NE(focused, nullptr);
    EXPECT_EQ(focused->id, kernel::display::kTerminalAppSurfaceId);
}

TEST(AppSurfaceTest, RegistryUpdatesBoundsAndVisibility)
{
    kernel::display::AppSurfaceRegistry registry;

    ASSERT_TRUE(registry.register_surface(
        kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                          {0, 0, 640, 480},
                                          true,
                                          true)));

    EXPECT_TRUE(registry.set_bounds(kernel::display::kTerminalAppSurfaceId, {16, 24, 320, 200}));
    const kernel::display::AppSurface * resized =
        registry.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(resized, nullptr);
    expect_rect(resized->bounds, 16, 24, 320, 200);
    expect_rect(resized->composited_surface().bounds, 16, 24, 320, 200);

    EXPECT_TRUE(registry.set_visible(kernel::display::kTerminalAppSurfaceId, false));
    const kernel::display::AppSurface * hidden =
        registry.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(hidden, nullptr);
    EXPECT_TRUE(hidden->hidden());
    EXPECT_FALSE(hidden->visible());
    EXPECT_FALSE(hidden->focused);
    EXPECT_FALSE(hidden->active);
    EXPECT_FALSE(hidden->composited_surface().layer().visible);
    EXPECT_EQ(registry.focused_surface(), nullptr);

    EXPECT_TRUE(registry.set_visible(kernel::display::kTerminalAppSurfaceId, true));
    EXPECT_TRUE(registry.set_focused(kernel::display::kTerminalAppSurfaceId));
    const kernel::display::AppSurface * visible =
        registry.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(visible, nullptr);
    EXPECT_TRUE(visible->open());
    EXPECT_TRUE(visible->focused);
    EXPECT_TRUE(visible->active);
}

TEST(AppSurfaceTest, RegistryRestoreKeepsExactStoredState)
{
    kernel::display::AppSurfaceRegistry registry;
    ASSERT_TRUE(registry.register_surface(
        kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                          {0, 0, 640, 480},
                                          true,
                                          true,
                                          true)));

    kernel::display::AppSurface restored =
        kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                          {16, 24, 320, 200},
                                          true,
                                          false,
                                          true);
    ASSERT_TRUE(registry.restore_surface(restored));

    const kernel::display::AppSurface * surface =
        registry.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(surface, nullptr);
    EXPECT_EQ(surface->bounds.x, 16u);
    EXPECT_EQ(surface->bounds.y, 24u);
    EXPECT_FALSE(surface->focused);
    EXPECT_TRUE(surface->active);
}

TEST(AppSurfaceTest, RegistryRejectsFocusOnHiddenOrClosedSurface)
{
    kernel::display::AppSurfaceRegistry registry;

    ASSERT_TRUE(registry.register_surface(
        kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                          {0, 0, 640, 480},
                                          false)));

    EXPECT_FALSE(registry.set_focused(kernel::display::kTerminalAppSurfaceId));
    EXPECT_TRUE(registry.set_visible(kernel::display::kTerminalAppSurfaceId, true));
    EXPECT_TRUE(registry.set_focused(kernel::display::kTerminalAppSurfaceId));
    EXPECT_TRUE(registry.close_surface(kernel::display::kTerminalAppSurfaceId));

    const kernel::display::AppSurface * closed =
        registry.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(closed, nullptr);
    EXPECT_TRUE(closed->closed());
    EXPECT_FALSE(closed->focused);
    EXPECT_FALSE(closed->active);
    EXPECT_FALSE(closed->composited_surface().valid());
    EXPECT_FALSE(closed->display_target().valid());
    EXPECT_FALSE(registry.set_focused(kernel::display::kTerminalAppSurfaceId));
    EXPECT_FALSE(registry.set_visible(kernel::display::kTerminalAppSurfaceId, true));
    EXPECT_FALSE(registry.set_bounds(kernel::display::kTerminalAppSurfaceId, {0, 0, 10, 10}));
}

TEST(AppSurfaceTest, RegistryRejectsDuplicateAndFullRegistrations)
{
    kernel::display::AppSurfaceRegistry registry;

    ASSERT_TRUE(registry.register_surface(
        kernel::display::make_app_surface(1, {0, 0, 10, 10})));
    EXPECT_FALSE(registry.register_surface(
        kernel::display::make_app_surface(1, {0, 0, 10, 10})));

    for (kernel::display::AppSurfaceId id = 2; id <= kernel::display::kMaxAppSurfaces; ++id)
    {
        ASSERT_TRUE(registry.register_surface(
            kernel::display::make_app_surface(id, {0, 0, 10, 10})));
    }

    EXPECT_EQ(registry.size(), registry.capacity());
    EXPECT_FALSE(registry.register_surface(
        kernel::display::make_app_surface(99, {0, 0, 10, 10})));
}
