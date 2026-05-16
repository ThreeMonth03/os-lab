#include <gtest/gtest.h>

#include "kernel/input/input_router.hpp"

namespace
{

kernel::input::Event key_event()
{
    kernel::input::Event event;
    event.kind = kernel::input::EventKind::Key;
    event.key.key = kernel::keyboard::Key::Character;
    event.key.character = 'x';
    event.key.pressed = true;
    return event;
}

kernel::input::Event mouse_move_event()
{
    kernel::input::Event event;
    event.kind = kernel::input::EventKind::MouseMove;
    event.mouse_move.delta_x = 3;
    event.mouse_move.delta_y = -2;
    return event;
}

TEST(InputRouterTest, DefaultsFocusToTerminalApp)
{
    const kernel::input::InputRouter router;

    EXPECT_EQ(router.focus(), kernel::input::InputFocus::TerminalApp);
}

TEST(InputRouterTest, RoutesKeyboardEventToFocusedTerminalApp)
{
    const kernel::input::InputRouter router;

    const kernel::input::RoutedEvent routed = router.route(key_event());

    EXPECT_EQ(routed.target, kernel::input::EventTarget::TerminalApp);
    EXPECT_EQ(routed.event.kind, kernel::input::EventKind::Key);
}

TEST(InputRouterTest, RoutesKeyboardEventToShellWhenShellFocusIsExplicit)
{
    kernel::input::InputRouter router;
    router.set_focus(kernel::input::InputFocus::Shell);

    const kernel::input::RoutedEvent routed = router.route(key_event());

    EXPECT_EQ(routed.target, kernel::input::EventTarget::Shell);
    EXPECT_EQ(routed.event.kind, kernel::input::EventKind::Key);
}

TEST(InputRouterTest, RoutesMouseMoveToPointer)
{
    const kernel::input::InputRouter router;

    const kernel::input::RoutedEvent routed = router.route(mouse_move_event());

    EXPECT_EQ(routed.target, kernel::input::EventTarget::Pointer);
    EXPECT_EQ(routed.event.kind, kernel::input::EventKind::MouseMove);
}

TEST(InputRouterTest, MouseMoveDoesNotChangeKeyboardFocus)
{
    kernel::input::InputRouter router;
    router.set_focus(kernel::input::InputFocus::GuiSurface);

    const kernel::input::RoutedEvent routed = router.route(mouse_move_event());

    EXPECT_EQ(routed.target, kernel::input::EventTarget::Pointer);
    EXPECT_EQ(router.focus(), kernel::input::InputFocus::GuiSurface);
}

TEST(InputRouterTest, IgnoresNoneEvent)
{
    const kernel::input::InputRouter router;

    const kernel::input::RoutedEvent routed = router.route(kernel::input::Event{});

    EXPECT_EQ(routed.target, kernel::input::EventTarget::None);
}

TEST(InputRouterTest, CanDisableShellFocus)
{
    kernel::input::InputRouter router;
    router.set_focus(kernel::input::InputFocus::None);

    const kernel::input::RoutedEvent key = router.route(key_event());
    const kernel::input::RoutedEvent mouse = router.route(mouse_move_event());

    EXPECT_EQ(router.focus(), kernel::input::InputFocus::None);
    EXPECT_EQ(key.target, kernel::input::EventTarget::None);
    EXPECT_EQ(mouse.target, kernel::input::EventTarget::Pointer);
}

TEST(InputRouterTest, RoutesKeyboardEventToGuiSurfaceWhenFocused)
{
    kernel::input::InputRouter router;
    router.set_focus(kernel::input::InputFocus::GuiSurface);

    const kernel::input::RoutedEvent key = router.route(key_event());
    const kernel::input::RoutedEvent mouse = router.route(mouse_move_event());

    EXPECT_EQ(router.focus(), kernel::input::InputFocus::GuiSurface);
    EXPECT_EQ(key.target, kernel::input::EventTarget::GuiSurface);
    EXPECT_EQ(key.event.kind, kernel::input::EventKind::Key);
    EXPECT_EQ(mouse.target, kernel::input::EventTarget::Pointer);
}

} // namespace
