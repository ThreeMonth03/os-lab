#include "kernel/display/app_window.hpp"

#include <gtest/gtest.h>

namespace
{

kernel::display::WindowSessionBounds bounds_for(uint64_t x = 8)
{
    return {
        {x, 16, 320, 200},
        {x + 4, 32, 312, 180},
    };
}

kernel::display::WindowSession terminal_session()
{
    return kernel::display::make_terminal_window_session(kernel::display::kTerminalWindowSessionId,
                                                         kernel::display::kTerminalAppSurfaceId,
                                                         bounds_for(),
                                                         true,
                                                         true,
                                                         true,
                                                         true);
}

kernel::display::WindowSession dummy_session()
{
    return kernel::display::make_app_window_session(kernel::display::kDummyDebugWindowSessionId,
                                                    kernel::display::kDummyDebugAppSurfaceId,
                                                    bounds_for(48),
                                                    kernel::display::WindowSessionRole::DummyDebugApp,
                                                    true,
                                                    true,
                                                    false,
                                                    false);
}

} // namespace

TEST(AppWindowTest, BuildsTerminalWindowFromSession)
{
    const kernel::display::AppWindow window =
        kernel::display::app_window_for_session(terminal_session());

    EXPECT_TRUE(window.valid());
    EXPECT_EQ(window.id, kernel::display::kTerminalAppWindowId);
    EXPECT_EQ(window.session_id, kernel::display::kTerminalWindowSessionId);
    EXPECT_EQ(window.client.kind, kernel::display::AppWindowKind::Terminal);
    EXPECT_EQ(window.client.surface_id, kernel::display::kTerminalAppSurfaceId);
    EXPECT_EQ(window.title, kernel::display::AppWindowTitle::Terminal);
    EXPECT_TRUE(window.visible());
    EXPECT_TRUE(window.focused);
    EXPECT_TRUE(window.active);
}

TEST(AppWindowTest, BuildsDummyDebugAppWindowFromSession)
{
    const kernel::display::AppWindow window =
        kernel::display::app_window_for_session(dummy_session());

    EXPECT_TRUE(window.valid());
    EXPECT_EQ(window.id, kernel::display::kDummyDebugAppWindowId);
    EXPECT_EQ(window.session_id, kernel::display::kDummyDebugWindowSessionId);
    EXPECT_EQ(window.client.kind, kernel::display::AppWindowKind::DummyDebugApp);
    EXPECT_EQ(window.client.surface_id, kernel::display::kDummyDebugAppSurfaceId);
    EXPECT_EQ(window.title, kernel::display::AppWindowTitle::DummyDebugApp);
}

TEST(AppWindowTest, RegistryRegistersFindsTerminalAndDummyWindows)
{
    kernel::display::AppWindowRegistry registry;

    ASSERT_TRUE(registry.register_window(kernel::display::app_window_for_session(terminal_session())));
    ASSERT_TRUE(registry.register_window(kernel::display::app_window_for_session(dummy_session())));

    EXPECT_EQ(registry.size(), 2u);
    EXPECT_NE(registry.find(kernel::display::kTerminalAppWindowId), nullptr);
    EXPECT_NE(registry.find(kernel::display::kDummyDebugAppWindowId), nullptr);
    EXPECT_EQ(registry.find_by_session(kernel::display::kDummyDebugWindowSessionId)->client.kind,
              kernel::display::AppWindowKind::DummyDebugApp);
    EXPECT_EQ(registry.find_by_surface(kernel::display::kTerminalAppSurfaceId)->client.kind,
              kernel::display::AppWindowKind::Terminal);
}

TEST(AppWindowTest, RegistryRejectsDuplicateAndFullWindows)
{
    kernel::display::AppWindowRegistry registry;
    ASSERT_TRUE(registry.register_window(kernel::display::app_window_for_session(terminal_session())));
    EXPECT_FALSE(registry.register_window(kernel::display::app_window_for_session(terminal_session())));

    ASSERT_TRUE(registry.register_window(kernel::display::app_window_for_session(dummy_session())));
    for (kernel::display::WindowSessionId id = 3; id <= kernel::display::kMaxAppWindows; ++id)
    {
        ASSERT_TRUE(registry.register_window(kernel::display::app_window_for_session(
            kernel::display::make_app_window_session(id,
                                                     static_cast<kernel::display::AppSurfaceId>(id),
                                                     bounds_for(static_cast<uint64_t>(id) * 24),
                                                     kernel::display::WindowSessionRole::DummyDebugApp,
                                                     true))));
    }

    EXPECT_FALSE(registry.register_window(kernel::display::app_window_for_session(
        kernel::display::make_app_window_session(10,
                                                 10,
                                                 bounds_for(10),
                                                 kernel::display::WindowSessionRole::DummyDebugApp,
                                                 true))));
}

TEST(AppWindowTest, RegistryTracksVisibleHiddenAndClosedState)
{
    kernel::display::AppWindowRegistry registry;
    ASSERT_TRUE(registry.register_window(kernel::display::app_window_for_session(terminal_session())));

    ASSERT_TRUE(registry.set_visible(kernel::display::kTerminalAppWindowId, false));
    const kernel::display::AppWindow * hidden =
        registry.find(kernel::display::kTerminalAppWindowId);
    ASSERT_NE(hidden, nullptr);
    EXPECT_TRUE(hidden->hidden());
    EXPECT_FALSE(hidden->focused);
    EXPECT_FALSE(hidden->active);

    ASSERT_TRUE(registry.set_visible(kernel::display::kTerminalAppWindowId, true));
    ASSERT_TRUE(registry.close_window(kernel::display::kTerminalAppWindowId));
    const kernel::display::AppWindow * closed =
        registry.find(kernel::display::kTerminalAppWindowId);
    ASSERT_NE(closed, nullptr);
    EXPECT_TRUE(closed->closed());
    EXPECT_FALSE(registry.set_visible(kernel::display::kTerminalAppWindowId, true));
}

TEST(AppWindowTest, RegistrySyncsFromSession)
{
    kernel::display::AppWindowRegistry registry;
    ASSERT_TRUE(registry.register_window(kernel::display::app_window_for_session(terminal_session())));

    kernel::display::WindowSession updated = terminal_session();
    updated.state = kernel::display::WindowSessionState::Hidden;
    updated.focused = false;
    updated.active = false;
    ASSERT_TRUE(registry.sync_session(updated));

    const kernel::display::AppWindow * window =
        registry.find(kernel::display::kTerminalAppWindowId);
    ASSERT_NE(window, nullptr);
    EXPECT_TRUE(window->hidden());
    EXPECT_FALSE(window->focused);
    EXPECT_FALSE(window->active);
}
