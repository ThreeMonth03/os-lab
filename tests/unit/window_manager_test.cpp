#include <gtest/gtest.h>

#include "kernel/display/desktop_shell.hpp"
#include "kernel/display/window_manager.hpp"

namespace
{

kernel::display::WindowSessionBounds bounds_for(kernel::display::Rect outer = {10, 20, 320, 200})
{
    return {
        outer,
        {outer.x + 4, outer.y + 16, outer.width - 8, outer.height - 20},
    };
}

kernel::display::WindowSession make_session(bool visible = true,
                                            bool focused = true,
                                            bool active = true,
                                            kernel::display::Rect outer = {10, 20, 320, 200})
{
    return kernel::display::make_terminal_window_session(kernel::display::kTerminalWindowSessionId,
                                                         kernel::display::kTerminalAppSurfaceId,
                                                         bounds_for(outer),
                                                         true,
                                                         visible,
                                                         focused,
                                                         active);
}

struct ManagerFixture
{
    kernel::display::WindowSessionRegistry sessions;
    kernel::display::AppSurfaceRegistry app_surfaces;
    kernel::display::DisplayTargetRegistry targets;
    kernel::display::AppSurfaceHost app_host;
    kernel::display::WindowSessionHost session_host;
    kernel::display::WindowStack stack;
    kernel::display::WindowManager manager;

    bool register_terminal(kernel::display::WindowSession session = make_session())
    {
        if (!sessions.register_session(session))
        {
            return false;
        }

        const kernel::display::AppSurface surface = session.app_surface();
        if (!app_surfaces.register_surface(surface) || !targets.register_target(surface.display_target()))
        {
            return false;
        }

        if (surface.focused)
        {
            if (!app_surfaces.set_focused(surface.id) ||
                !targets.set_focused(surface.display_surface_id) ||
                !targets.set_active(surface.display_surface_id))
            {
                return false;
            }
        }

        if (!stack.register_window(session))
        {
            return false;
        }

        app_host.reset(app_surfaces, targets);
        session_host.reset(sessions, app_host);
        manager.reset(session_host, stack);
        return true;
    }
};

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

} // namespace

TEST(WindowManagerTest, ShowHiddenWindowRestoresVisibility)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal(make_session(false, false, false)));

    kernel::display::WindowManagerResult result;
    ASSERT_TRUE(fixture.manager.show_window(kernel::display::kTerminalWindowSessionId, result));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(result.valid());
    EXPECT_TRUE(session->visible());
    EXPECT_FALSE(session->focused);
    EXPECT_FALSE(session->active);
}

TEST(WindowManagerTest, HideVisibleWindowClearsFocusAndActive)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowManagerResult result;
    ASSERT_TRUE(fixture.manager.hide_window(kernel::display::kTerminalWindowSessionId, result));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(result.valid());
    EXPECT_TRUE(session->hidden());
    EXPECT_FALSE(session->focused);
    EXPECT_FALSE(session->active);
    EXPECT_EQ(fixture.stack.focused_window(), kernel::display::kInvalidWindowSessionId);
    EXPECT_EQ(fixture.stack.active_window(), kernel::display::kInvalidWindowSessionId);
}

TEST(WindowManagerTest, FocusActivateRaiseVisibleWindow)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal(make_session(true, false, false)));

    kernel::display::WindowManagerResult result;
    ASSERT_TRUE(fixture.manager.focus_activate_raise_window(kernel::display::kTerminalWindowSessionId,
                                                            result));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(result.valid());
    EXPECT_TRUE(session->focused);
    EXPECT_TRUE(session->active);
    EXPECT_TRUE(fixture.manager.selected(kernel::display::kTerminalWindowSessionId));
}

TEST(WindowManagerTest, RejectsFocusHiddenOrClosedWindow)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal(make_session(false, false, false)));

    kernel::display::WindowManagerResult result;
    EXPECT_FALSE(fixture.manager.focus_window(kernel::display::kTerminalWindowSessionId, result));

    ASSERT_TRUE(fixture.manager.show_window(kernel::display::kTerminalWindowSessionId, result));
    ASSERT_TRUE(fixture.manager.close_window(kernel::display::kTerminalWindowSessionId, result));
    EXPECT_FALSE(fixture.manager.focus_window(kernel::display::kTerminalWindowSessionId, result));
}

TEST(WindowManagerTest, MoveUpdatesSessionSurfaceAndDescriptorWithoutResize)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowManagerResult result;
    ASSERT_TRUE(fixture.manager.move_window(kernel::display::kTerminalWindowSessionId,
                                            bounds_for({40, 50, 320, 200}),
                                            result));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    const kernel::display::AppSurface * app =
        fixture.app_surfaces.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(session, nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_TRUE(result.valid());
    expect_rect(session->bounds.outer, 40, 50, 320, 200);
    expect_rect(app->bounds, 40, 50, 320, 200);
    expect_rect(session->composited_surface().bounds, 40, 50, 320, 200);
}

TEST(WindowManagerTest, ResizeUpdatesSessionSurfaceAndDescriptor)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowManagerResult result;
    ASSERT_TRUE(fixture.manager.resize_window(kernel::display::kTerminalWindowSessionId,
                                              bounds_for({10, 20, 480, 300}),
                                              result));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    const kernel::display::AppSurface * app =
        fixture.app_surfaces.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(session, nullptr);
    ASSERT_NE(app, nullptr);
    EXPECT_TRUE(result.valid());
    expect_rect(session->bounds.outer, 10, 20, 480, 300);
    expect_rect(app->bounds, 10, 20, 480, 300);
    expect_rect(session->composited_surface().bounds, 10, 20, 480, 300);
}

TEST(WindowManagerTest, ResizeFailureRollsBackSessionAndAppState)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());
    fixture.targets.clear();

    kernel::display::WindowManagerResult result;
    EXPECT_FALSE(fixture.manager.resize_window(kernel::display::kTerminalWindowSessionId,
                                               bounds_for({10, 20, 480, 300}),
                                               result));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    const kernel::display::AppSurface * app =
        fixture.app_surfaces.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(session, nullptr);
    ASSERT_NE(app, nullptr);
    expect_rect(session->bounds.outer, 10, 20, 320, 200);
    expect_rect(app->bounds, 10, 20, 320, 200);
}

TEST(WindowManagerTest, CloseRemainsDistinctFromHide)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowManagerResult result;
    ASSERT_TRUE(fixture.manager.close_window(kernel::display::kTerminalWindowSessionId, result));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(result.valid());
    EXPECT_TRUE(session->closed());
    EXPECT_FALSE(session->focused);
    EXPECT_FALSE(session->active);
    EXPECT_FALSE(fixture.manager.show_focus_activate_raise_window(kernel::display::kTerminalWindowSessionId,
                                                                  result));
}

TEST(WindowManagerTest, SnapshotKeepsPolicyTermsSeparate)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    const kernel::display::WindowManagerSnapshot snapshot = fixture.manager.snapshot();
    EXPECT_EQ(snapshot.focused, kernel::display::kTerminalWindowSessionId);
    EXPECT_EQ(snapshot.active, kernel::display::kTerminalWindowSessionId);
    EXPECT_EQ(snapshot.topmost_visible, kernel::display::kTerminalWindowSessionId);
    EXPECT_EQ(snapshot.window_count, 1u);
}

TEST(WindowManagerTest, DesktopShellTerminalCommandGoesThroughManager)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal(make_session(false, false, false)));

    const kernel::display::desktop_shell::WindowCommand command =
        kernel::display::desktop_shell::ActionHandler::command_for(
            kernel::display::desktop_bar::DesktopShellAction::TerminalShowFocus);
    ASSERT_EQ(command, kernel::display::desktop_shell::WindowCommand::TerminalShowFocusRaise);

    kernel::display::WindowManagerResult result;
    ASSERT_TRUE(fixture.manager.show_focus_activate_raise_window(kernel::display::kTerminalWindowSessionId,
                                                                 result));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(result.valid());
    EXPECT_TRUE(session->visible());
    EXPECT_TRUE(session->focused);
    EXPECT_TRUE(session->active);
    EXPECT_EQ(fixture.stack.topmost_visible_window(), kernel::display::kTerminalWindowSessionId);
}

TEST(WindowManagerTest, DesktopShellTerminalCommandDoesNotReopenClosedWindow)
{
    ManagerFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowManagerResult result;
    ASSERT_TRUE(fixture.manager.close_window(kernel::display::kTerminalWindowSessionId, result));

    EXPECT_FALSE(fixture.manager.show_focus_activate_raise_window(kernel::display::kTerminalWindowSessionId,
                                                                  result));
}
