#include "kernel/shell/shell.hpp"

#include <stddef.h>
#include <stdint.h>

#include "shell_command_executor.hpp"
#include "shell_editor_view.hpp"

#include "kernel/input/input_router.hpp"
#include "kernel/input/input.hpp"
#include "kernel/input/keyboard.hpp"
#include "kernel/debug/display_profile.hpp"
#include "kernel/display/display_runtime.hpp"
#include "kernel/display/mouse_cursor.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/base/string_view.hpp"
#include "kernel/console/terminal.hpp"
#include "kernel/text/history.hpp"
#include "kernel/text/line_editor.hpp"

#ifndef OS_LAB_DISPLAY_PROFILING
#define OS_LAB_DISPLAY_PROFILING 0
#endif

#ifndef OS_LAB_DISPLAY_PROFILE_SCRIPT
#define OS_LAB_DISPLAY_PROFILE_SCRIPT 0
#endif

namespace
{

namespace mouse_cursor = kernel::display::mouse_cursor;
namespace display_runtime = kernel::display::runtime;
namespace serial = kernel::drivers::serial;
namespace terminal = kernel::console::terminal;

#if OS_LAB_DISPLAY_PROFILING

void finish_display_profile_after_frame()
{
    kernel::debug::finish_display_profile_command(display_runtime::stats());
}

#else

void finish_display_profile_after_frame() {}

#endif

char lowercase(char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return static_cast<char>(value - 'A' + 'a');
    }

    return value;
}

bool handle_control_shortcut(const kernel::input::keyboard::KeyEvent & event, kernel::text::LineEditor & line, kernel::shell::EditorView & view, bool caps_lock, kernel::text::History & history)
{
    if (!event.control || event.key != kernel::input::keyboard::Key::Character)
    {
        return false;
    }

    switch (lowercase(event.character))
    {
    case 'a':
    {
        const kernel::text::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.move_to_start())
        {
            view.redraw_change(line, caps_lock, kernel::text::EditorEditKind::CursorMove, before);
        }
        break;
    }
    case 'c':
        terminal::hide_cursor();
        view.move_to_line_end(line, caps_lock);
        terminal::write_char('\n');
        terminal::write_line("cancelled");
        line.clear();
        history.reset_browse();
        view.write_new_prompt_and_line(line, caps_lock);
        break;
    case 'e':
    {
        const kernel::text::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.move_to_end())
        {
            view.redraw_change(line, caps_lock, kernel::text::EditorEditKind::CursorMove, before);
        }
        break;
    }
    case 'l':
        terminal::hide_cursor();
        terminal::clear();
        view.write_new_prompt_and_line(line, caps_lock);
        break;
    case 'u':
        line.clear();
        history.reset_browse();
        view.redraw_prompt_and_line(line, caps_lock);
        break;
    default:
        break;
    }

    return true;
}

void handle_key_event(const kernel::input::keyboard::KeyEvent & event, kernel::text::LineEditor & line, kernel::shell::EditorView & view, bool & caps_lock, kernel::text::History & history)
{
    if (!event.pressed)
    {
        return;
    }

    if (handle_control_shortcut(event, line, view, caps_lock, history))
    {
        return;
    }

    switch (event.key)
    {
    case kernel::input::keyboard::Key::Character:
    {
        const kernel::text::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.insert(event.character))
        {
            history.reset_browse();
            view.redraw_change(line, caps_lock, kernel::text::EditorEditKind::Insert, before);
        }
        break;
    }
    case kernel::input::keyboard::Key::Tab:
    {
        const kernel::text::EditorSnapshot before = view.snapshot(line, caps_lock);
        const size_t spaces = kernel::text::LineEditor::spaces_to_next_tab_stop(line.cursor());
        if (line.insert_spaces(spaces))
        {
            history.reset_browse();
            view.redraw_change(line, caps_lock, kernel::text::EditorEditKind::Insert, before);
        }
        break;
    }
    case kernel::input::keyboard::Key::Backspace:
    {
        const kernel::text::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.backspace())
        {
            history.reset_browse();
            view.redraw_change(line, caps_lock, kernel::text::EditorEditKind::Backspace, before);
        }
        break;
    }
    case kernel::input::keyboard::Key::Delete:
    {
        const kernel::text::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.delete_forward())
        {
            history.reset_browse();
            view.redraw_change(line, caps_lock, kernel::text::EditorEditKind::DeleteForward, before);
        }
        break;
    }
    case kernel::input::keyboard::Key::UpArrow:
    {
        kernel::StringView command;
        view.redraw_history_result(history.previous(command), command, line, caps_lock);
        break;
    }
    case kernel::input::keyboard::Key::DownArrow:
    {
        kernel::StringView command;
        view.redraw_history_result(history.next(command), command, line, caps_lock);
        break;
    }
    case kernel::input::keyboard::Key::LeftArrow:
    {
        const kernel::text::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.move_left())
        {
            view.redraw_change(line, caps_lock, kernel::text::EditorEditKind::CursorMove, before);
        }
        break;
    }
    case kernel::input::keyboard::Key::RightArrow:
    {
        const kernel::text::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.move_right())
        {
            view.redraw_change(line, caps_lock, kernel::text::EditorEditKind::CursorMove, before);
        }
        break;
    }
    case kernel::input::keyboard::Key::Enter:
    {
        terminal::hide_cursor();
        view.move_to_line_end(line, caps_lock);
        terminal::write_char('\n');
        if (!line.empty())
        {
            history.push(line.view());
        }
        kernel::shell::execute_command(line.view());
        line.clear();
        history.reset_browse();
        view.write_new_prompt_and_line(line, caps_lock);
        break;
    }
    case kernel::input::keyboard::Key::CapsLock:
    {
        const kernel::text::EditorSnapshot before = view.snapshot(line, caps_lock);
        caps_lock = event.caps_lock;
        view.redraw_change(line, caps_lock, kernel::text::EditorEditKind::PromptChange, before);
        break;
    }
    default:
        break;
    }
}

#if OS_LAB_DISPLAY_PROFILE_SCRIPT

kernel::input::keyboard::KeyEvent make_profile_script_character_event(char character, bool caps_lock)
{
    kernel::input::keyboard::KeyEvent event;
    event.key = kernel::input::keyboard::Key::Character;
    event.character = character;
    event.pressed = true;
    event.caps_lock = caps_lock;
    return event;
}

kernel::input::keyboard::KeyEvent make_profile_script_enter_event(bool caps_lock)
{
    kernel::input::keyboard::KeyEvent event;
    event.key = kernel::input::keyboard::Key::Enter;
    event.pressed = true;
    event.caps_lock = caps_lock;
    return event;
}

void dispatch_profile_script_key(const kernel::input::keyboard::KeyEvent & event,
                                 kernel::text::LineEditor & line,
                                 kernel::shell::EditorView & view,
                                 bool & caps_lock,
                                 kernel::text::History & history)
{
    {
        terminal::ScopedUpdate terminal_update;
        handle_key_event(event, line, view, caps_lock, history);
    }
    finish_display_profile_after_frame();
}

void type_profile_script_command(kernel::StringView command,
                                 kernel::text::LineEditor & line,
                                 kernel::shell::EditorView & view,
                                 bool & caps_lock,
                                 kernel::text::History & history)
{
    for (size_t index = 0; index < command.size(); ++index)
    {
        dispatch_profile_script_key(make_profile_script_character_event(command[index], caps_lock),
                                    line,
                                    view,
                                    caps_lock,
                                    history);
    }

    dispatch_profile_script_key(make_profile_script_enter_event(caps_lock), line, view, caps_lock, history);
}

void run_display_profile_script(kernel::text::LineEditor & line,
                                kernel::shell::EditorView & view,
                                bool & caps_lock,
                                kernel::text::History & history)
{
    constexpr kernel::StringView kProfileCommands[] = {
        "help",
        "input",
        "heap",
        "slab",
        "a",
    };

    serial::write_line("os-lab: display profile script running");
    for (kernel::StringView command : kProfileCommands)
    {
        type_profile_script_command(command, line, view, caps_lock, history);
    }
    serial::write_line("os-lab: display profile script done");
}

#endif

void handle_mouse_move_event(const kernel::input::MouseMoveEvent & event,
                             kernel::text::LineEditor & line,
                             kernel::shell::EditorView & view,
                             bool caps_lock)
{
    if (!event.x_overflow && !event.y_overflow)
    {
        mouse_cursor::move_by(event.delta_x, event.delta_y);
    }

    const mouse_cursor::Position position = mouse_cursor::position();
    display_runtime::update_pointer_target(position.x, position.y);
    const display_runtime::TerminalWindowInteractionResult window_result =
        display_runtime::handle_terminal_window_pointer(position.x, position.y, event.left_button);
    if (window_result.clear_keyboard_focus)
    {
        kernel::input::set_focus(kernel::input::InputFocus::None);
    }
    if (window_result.app_resized)
    {
        view.resynchronize_after_terminal_resize(line, caps_lock);
    }
}

void handle_routed_event(const kernel::input::RoutedEvent & routed, kernel::text::LineEditor & line, kernel::shell::EditorView & view, bool & caps_lock, kernel::text::History & history)
{
    switch (routed.target)
    {
    case kernel::input::EventTarget::TerminalApp:
    case kernel::input::EventTarget::Shell:
        if (routed.event.kind == kernel::input::EventKind::Key)
        {
            {
                terminal::ScopedUpdate terminal_update;
                handle_key_event(routed.event.key, line, view, caps_lock, history);
            }
            finish_display_profile_after_frame();
        }
        break;
    case kernel::input::EventTarget::GuiSurface:
        break;
    case kernel::input::EventTarget::Pointer:
        if (routed.event.kind == kernel::input::EventKind::MouseMove)
        {
            handle_mouse_move_event(routed.event.mouse_move, line, view, caps_lock);
        }
        break;
    case kernel::input::EventTarget::None:
        break;
    }
}

} // namespace

namespace kernel::shell
{

[[noreturn]] void run()
{
    text::LineEditor line;
    text::History history;
    EditorView view;
    bool caps_lock = false;

    {
        terminal::ScopedUpdate startup_update;
        terminal::write_line("");
        terminal::write_line("interactive input ready");
        execute_command("help");
        view.write_new_prompt_and_line(line, caps_lock);
    }
    finish_display_profile_after_frame();
    serial::write_line("os-lab: interactive terminal ready");

#if OS_LAB_DISPLAY_PROFILE_SCRIPT
    run_display_profile_script(line, view, caps_lock, history);
#endif

    while (true)
    {
        display_runtime::refresh_debug_overlay_if_due();

        input::Event event;
        if (input::poll(event))
        {
            handle_routed_event(input::route(event), line, view, caps_lock, history);
        }
        else
        {
            asm volatile("pause");
        }
    }
}

} // namespace kernel::shell
