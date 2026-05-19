#include <gtest/gtest.h>

#include "kernel/display/window_session.hpp"

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

struct HostFixture
{
    kernel::display::WindowSessionRegistry sessions;
    kernel::display::AppSurfaceRegistry app_surfaces;
    kernel::display::DisplayTargetRegistry targets;
    kernel::display::AppSurfaceHost app_host;
    kernel::display::WindowSessionHost session_host;

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

        app_host.reset(app_surfaces, targets);
        session_host.reset(sessions, app_host);
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

TEST(WindowSessionTest, RegistersTerminalSession)
{
    kernel::display::WindowSessionRegistry registry;
    ASSERT_TRUE(registry.register_session(make_session()));

    const kernel::display::WindowSession * session =
        registry.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(session->open());
    EXPECT_TRUE(session->focused);
    EXPECT_TRUE(session->active);
    EXPECT_TRUE(session->chrome_visible);
    EXPECT_EQ(session->role, kernel::display::WindowSessionRole::Terminal);
}

TEST(WindowSessionTest, ShowHideAndCloseStateTransitions)
{
    kernel::display::WindowSessionRegistry registry;
    ASSERT_TRUE(registry.register_session(make_session()));

    ASSERT_TRUE(registry.set_visible(kernel::display::kTerminalWindowSessionId, false));
    const kernel::display::WindowSession * hidden =
        registry.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(hidden, nullptr);
    EXPECT_TRUE(hidden->hidden());
    EXPECT_FALSE(hidden->focused);
    EXPECT_FALSE(hidden->active);
    EXPECT_EQ(registry.focused_session(), nullptr);
    EXPECT_EQ(registry.active_session(), nullptr);

    ASSERT_TRUE(registry.set_visible(kernel::display::kTerminalWindowSessionId, true));
    ASSERT_TRUE(registry.set_focused(kernel::display::kTerminalWindowSessionId));
    ASSERT_TRUE(registry.set_active(kernel::display::kTerminalWindowSessionId));

    ASSERT_TRUE(registry.close_session(kernel::display::kTerminalWindowSessionId));
    const kernel::display::WindowSession * closed =
        registry.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(closed, nullptr);
    EXPECT_TRUE(closed->closed());
    EXPECT_FALSE(closed->focused);
    EXPECT_FALSE(closed->active);
    EXPECT_FALSE(closed->composited_surface().valid());
}

TEST(WindowSessionTest, FocusAndActiveRejectHiddenOrClosedSessions)
{
    kernel::display::WindowSessionRegistry registry;
    ASSERT_TRUE(registry.register_session(make_session(false, false, false)));

    EXPECT_FALSE(registry.set_focused(kernel::display::kTerminalWindowSessionId));
    EXPECT_FALSE(registry.set_active(kernel::display::kTerminalWindowSessionId));

    ASSERT_TRUE(registry.set_visible(kernel::display::kTerminalWindowSessionId, true));
    ASSERT_TRUE(registry.close_session(kernel::display::kTerminalWindowSessionId));

    EXPECT_FALSE(registry.set_focused(kernel::display::kTerminalWindowSessionId));
    EXPECT_FALSE(registry.set_active(kernel::display::kTerminalWindowSessionId));
}

TEST(WindowSessionTest, VisibleSessionEmitsAppDescriptor)
{
    const kernel::display::WindowSession session = make_session();

    const kernel::display::CompositedSurfaceDescriptor descriptor =
        session.composited_surface();

    EXPECT_TRUE(descriptor.valid());
    EXPECT_EQ(descriptor.role, kernel::display::CompositedSurfaceRole::App);
    EXPECT_TRUE(descriptor.visible);
    EXPECT_TRUE(descriptor.active);
    EXPECT_TRUE(descriptor.focused);
}

TEST(WindowSessionTest, HiddenSessionEmitsInvisibleDescriptor)
{
    const kernel::display::WindowSession session = make_session(false, false, false);

    const kernel::display::CompositedSurfaceDescriptor descriptor =
        session.composited_surface();

    EXPECT_TRUE(descriptor.valid());
    EXPECT_FALSE(descriptor.visible);
    EXPECT_FALSE(descriptor.active);
    EXPECT_FALSE(descriptor.focused);
}

TEST(WindowSessionHostTest, MoveUpdatesSessionAndAppSurfaceWithoutChangingSize)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowSessionMutation mutation;
    ASSERT_TRUE(fixture.session_host.move_session(kernel::display::kTerminalWindowSessionId,
                                                  bounds_for({40, 50, 320, 200}),
                                                  mutation));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    expect_rect(session->bounds.outer, 40, 50, 320, 200);
    const kernel::display::AppSurface * app =
        fixture.app_surfaces.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(app, nullptr);
    expect_rect(app->bounds, 40, 50, 320, 200);
    expect_rect(mutation.repaint_bounds, 10, 20, 350, 230);
}

TEST(WindowSessionHostTest, ResizeUpdatesSessionAndAppSurface)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowSessionMutation mutation;
    ASSERT_TRUE(fixture.session_host.resize_session(kernel::display::kTerminalWindowSessionId,
                                                    bounds_for({10, 20, 480, 300}),
                                                    mutation));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    expect_rect(session->bounds.outer, 10, 20, 480, 300);
    expect_rect(session->bounds.client, 14, 36, 472, 280);
}

TEST(WindowSessionHostTest, ResizeFailureRollsBackSessionAndAppState)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());
    fixture.targets.clear();

    kernel::display::WindowSessionMutation mutation;
    EXPECT_FALSE(fixture.session_host.resize_session(kernel::display::kTerminalWindowSessionId,
                                                     bounds_for({10, 20, 480, 300}),
                                                     mutation));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    expect_rect(session->bounds.outer, 10, 20, 320, 200);
    const kernel::display::AppSurface * app =
        fixture.app_surfaces.find(kernel::display::kTerminalAppSurfaceId);
    ASSERT_NE(app, nullptr);
    expect_rect(app->bounds, 10, 20, 320, 200);
}

TEST(WindowSessionHostTest, HideClearsFocusAndActive)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowSessionMutation mutation;
    ASSERT_TRUE(fixture.session_host.set_visible(kernel::display::kTerminalWindowSessionId,
                                                 false,
                                                 mutation));

    const kernel::display::WindowSession * session =
        fixture.sessions.find(kernel::display::kTerminalWindowSessionId);
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(session->hidden());
    EXPECT_FALSE(session->focused);
    EXPECT_FALSE(session->active);
    EXPECT_EQ(fixture.sessions.focused_session(), nullptr);
    EXPECT_EQ(fixture.sessions.active_session(), nullptr);
}

TEST(WindowSessionHostTest, ShowAndFocusRestoresVisibleAndFocusedOnly)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowSessionMutation mutation;
    ASSERT_TRUE(fixture.session_host.set_visible(kernel::display::kTerminalWindowSessionId,
                                                 false,
                                                 mutation));
    ASSERT_TRUE(fixture.session_host.set_visible(kernel::display::kTerminalWindowSessionId,
                                                 true,
                                                 mutation));
    EXPECT_TRUE(mutation.current.open());
    EXPECT_FALSE(mutation.current.focused);
    EXPECT_FALSE(mutation.current.active);

    ASSERT_TRUE(fixture.session_host.focus_session(kernel::display::kTerminalWindowSessionId,
                                                   mutation));
    EXPECT_TRUE(mutation.current.focused);
    EXPECT_FALSE(mutation.current.active);
}

TEST(WindowSessionHostTest, CloseKeepsSemanticsDistinctFromHide)
{
    HostFixture fixture;
    ASSERT_TRUE(fixture.register_terminal());

    kernel::display::WindowSessionMutation mutation;
    ASSERT_TRUE(fixture.session_host.close_session(kernel::display::kTerminalWindowSessionId,
                                                   mutation));

    EXPECT_TRUE(mutation.current.closed());
    EXPECT_FALSE(mutation.current.visible());
    EXPECT_FALSE(mutation.current.focused);
    EXPECT_FALSE(mutation.current.active);
    EXPECT_FALSE(fixture.session_host.set_visible(kernel::display::kTerminalWindowSessionId,
                                                  true,
                                                  mutation));
}
