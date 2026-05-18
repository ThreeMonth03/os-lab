#include <gtest/gtest.h>

#include "kernel/display/app_surface_host.hpp"

namespace
{

void expect_rect(kernel::display::Rect actual,
                 uint64_t x,
                 uint64_t y,
                 uint64_t width,
                 uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

struct HostFixture
{
    kernel::display::AppSurfaceRegistry app_surfaces;
    kernel::display::DisplayTargetRegistry targets;
    kernel::display::AppSurfaceHost host;

    bool register_terminal(kernel::display::Rect bounds = {0, 0, 640, 480},
                           bool visible = true,
                           bool focused = true)
    {
        const kernel::display::AppSurface surface =
            kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                              bounds,
                                              visible,
                                              focused);
        if (!app_surfaces.register_surface(surface) || !targets.register_target(surface.display_target()))
        {
            return false;
        }

        if (focused)
        {
            if (!app_surfaces.set_focused(surface.id) ||
                !targets.set_focused(surface.display_surface_id) ||
                !targets.set_active(surface.display_surface_id))
            {
                return false;
            }
        }

        host.reset(app_surfaces, targets);
        return true;
    }
};

} // namespace

TEST(AppSurfaceHostTest, ResizeUpdatesRegistryAndDisplayTarget)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::AppSurfaceMutation mutation;
    ASSERT_TRUE(fixture.host.resize_surface(kernel::display::kTerminalAppSurfaceId,
                                            {16, 32, 320, 200},
                                            mutation));

    const kernel::display::AppSurface * surface =
        fixture.app_surfaces.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(surface, nullptr);
    expect_rect(surface->bounds, 16, 32, 320, 200);
    expect_rect(mutation.previous.bounds, 0, 0, 640, 480);
    expect_rect(mutation.current.bounds, 16, 32, 320, 200);
    expect_rect(mutation.repaint_bounds, 0, 0, 640, 480);

    const kernel::display::SurfaceDescriptor * target =
        fixture.targets.find(surface->display_surface_id);
    ASSERT_NE(target, nullptr);
    expect_rect(target->bounds, 16, 32, 320, 200);
    EXPECT_TRUE(target->focused);
}

TEST(AppSurfaceHostTest, ResizeFailureKeepsOldStateValid)
{
    kernel::display::AppSurfaceRegistry app_surfaces;
    kernel::display::DisplayTargetRegistry targets;
    kernel::display::AppSurfaceHost host;
    ASSERT_TRUE(app_surfaces.register_surface(
        kernel::display::make_app_surface(kernel::display::kTerminalAppSurfaceId,
                                          {0, 0, 640, 480},
                                          true,
                                          true)));
    host.reset(app_surfaces, targets);

    kernel::display::AppSurfaceMutation mutation;
    EXPECT_FALSE(host.resize_surface(kernel::display::kTerminalAppSurfaceId,
                                     {16, 32, 320, 200},
                                     mutation));

    const kernel::display::AppSurface * surface =
        app_surfaces.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(surface, nullptr);
    expect_rect(surface->bounds, 0, 0, 640, 480);
    EXPECT_TRUE(surface->open());
}

TEST(AppSurfaceHostTest, HidingSurfaceClearsFocus)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::AppSurfaceMutation mutation;
    ASSERT_TRUE(fixture.host.set_visible(kernel::display::kTerminalAppSurfaceId,
                                         false,
                                         mutation));

    const kernel::display::AppSurface * surface =
        fixture.app_surfaces.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(surface, nullptr);
    EXPECT_TRUE(surface->hidden());
    EXPECT_FALSE(surface->focused);
    EXPECT_EQ(fixture.app_surfaces.focused_surface(), nullptr);
    EXPECT_EQ(fixture.targets.focused_target_id(), kernel::display::kInvalidSurfaceId);
}

TEST(AppSurfaceHostTest, FocusHiddenSurfaceFails)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal({0, 0, 640, 480}, false, false));

    kernel::display::AppSurfaceMutation mutation;
    EXPECT_FALSE(fixture.host.focus_surface(kernel::display::kTerminalAppSurfaceId, mutation));
    EXPECT_EQ(fixture.app_surfaces.focused_surface(), nullptr);
}

TEST(AppSurfaceHostTest, CloseSurfaceClearsActiveAndFocus)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::AppSurfaceMutation mutation;
    ASSERT_TRUE(fixture.host.close_surface(kernel::display::kTerminalAppSurfaceId, mutation));

    const kernel::display::AppSurface * surface =
        fixture.app_surfaces.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(surface, nullptr);
    EXPECT_TRUE(surface->closed());
    EXPECT_FALSE(surface->composited_surface().valid());
    EXPECT_EQ(fixture.app_surfaces.focused_surface(), nullptr);
    EXPECT_EQ(fixture.targets.active_target_id(), kernel::display::kInvalidSurfaceId);
    EXPECT_EQ(fixture.targets.focused_target_id(), kernel::display::kInvalidSurfaceId);
    expect_rect(mutation.repaint_bounds, 0, 0, 640, 480);
}
