#include <gtest/gtest.h>

#include "kernel/display/gui_surface.hpp"

namespace
{

kernel::display::GuiSurface surface(kernel::display::GuiSurfaceId id)
{
    return kernel::display::make_gui_surface(id, {10, 20, 100, 50}, false, true);
}

} // namespace

TEST(GuiSurfaceTest, BuildsDisplayTargetAndLayerDescriptors)
{
    const kernel::display::GuiSurface gui = kernel::display::make_gui_surface(1, {10, 20, 100, 50}, true, true);

    EXPECT_TRUE(gui.valid());
    EXPECT_EQ(gui.display_surface_id, kernel::display::display_surface_id_for(1));

    const kernel::display::SurfaceDescriptor target = gui.display_target();
    EXPECT_EQ(target.id, gui.display_surface_id);
    EXPECT_EQ(target.kind, kernel::display::DisplayTargetKind::GuiSurface);
    EXPECT_EQ(target.bounds.x, 10u);
    EXPECT_FALSE(target.active);
    EXPECT_FALSE(target.focused);

    const kernel::display::Layer layer = gui.composited_surface().layer();
    EXPECT_EQ(layer.kind, kernel::display::LayerKind::GuiSurface);
    EXPECT_EQ(layer.surface_id, gui.display_surface_id);
    EXPECT_TRUE(layer.visible);
    EXPECT_TRUE(layer.occludes_lower_repaint());
}

TEST(GuiSurfaceTest, ValidatesSurfaceState)
{
    EXPECT_FALSE(kernel::display::GuiSurface{}.valid());
    EXPECT_FALSE(kernel::display::make_gui_surface(1, {0, 0, 0, 50}, true, true).valid());

    kernel::display::GuiSurface focused_without_focusable =
        kernel::display::make_gui_surface(1, {0, 0, 10, 10}, true, false);
    focused_without_focusable.focused = true;
    EXPECT_FALSE(focused_without_focusable.valid());
}

TEST(GuiSurfaceTest, RegistryRegistersAndFindsSurfaces)
{
    kernel::display::GuiSurfaceRegistry registry;

    ASSERT_TRUE(registry.register_surface(surface(1)));
    ASSERT_TRUE(registry.register_surface(surface(2)));

    EXPECT_EQ(registry.size(), 2u);
    ASSERT_NE(registry.find(1), nullptr);
    ASSERT_NE(registry.find_by_display_surface(kernel::display::display_surface_id_for(2)), nullptr);
    EXPECT_EQ(registry.find(42), nullptr);
}

TEST(GuiSurfaceTest, RegistryRejectsInvalidDuplicateAndFull)
{
    kernel::display::GuiSurfaceRegistry registry;

    EXPECT_FALSE(registry.register_surface(kernel::display::GuiSurface{}));
    ASSERT_TRUE(registry.register_surface(surface(1)));
    EXPECT_FALSE(registry.register_surface(surface(1)));

    kernel::display::GuiSurface duplicate_display = surface(2);
    duplicate_display.display_surface_id = kernel::display::display_surface_id_for(1);
    EXPECT_FALSE(registry.register_surface(duplicate_display));

    for (size_t id = 2; id <= registry.capacity(); ++id)
    {
        ASSERT_TRUE(registry.register_surface(surface(static_cast<kernel::display::GuiSurfaceId>(id))));
    }
    EXPECT_EQ(registry.size(), registry.capacity());
    EXPECT_FALSE(registry.register_surface(surface(99)));
}

TEST(GuiSurfaceTest, RegistryTracksFocusableSurface)
{
    kernel::display::GuiSurfaceRegistry registry;
    kernel::display::GuiSurface not_focusable =
        kernel::display::make_gui_surface(1, {0, 0, 10, 10}, true, false);

    ASSERT_TRUE(registry.register_surface(not_focusable));
    ASSERT_TRUE(registry.register_surface(surface(2)));

    EXPECT_FALSE(registry.set_focused(1));
    EXPECT_TRUE(registry.set_focused(2));

    const kernel::display::GuiSurface * focused = registry.focused_surface();
    ASSERT_NE(focused, nullptr);
    EXPECT_EQ(focused->id, 2);

    registry.clear_focus();
    EXPECT_EQ(registry.focused_surface(), nullptr);
}
