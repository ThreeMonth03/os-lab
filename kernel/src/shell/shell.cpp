#include "kernel/shell/shell.hpp"

#include <stddef.h>
#include <stdint.h>

#include "shell_command_executor.hpp"
#include "shell_editor_view.hpp"

#include "kernel/input/input.hpp"
#include "kernel/input/keyboard.hpp"
#include "kernel/display/mouse_cursor.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/base/string_view.hpp"
#include "kernel/console/terminal.hpp"
#include "kernel/text/history.hpp"
#include "kernel/text/line_editor.hpp"

namespace {

char lowercase(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }

    return value;
}

bool handle_control_shortcut(const kernel::keyboard::KeyEvent& event, kernel::LineEditor& line,
                             kernel::shell::EditorView& view, bool caps_lock,
                             kernel::History& history) {
    if (!event.control || event.key != kernel::keyboard::Key::Character) {
        return false;
    }

    switch (lowercase(event.character)) {
    case 'a': {
        const kernel::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.move_to_start()) {
            view.redraw_change(line, caps_lock, kernel::EditorEditKind::CursorMove, before);
        }
        break;
    }
    case 'c':
        kernel::terminal::hide_cursor();
        view.move_to_line_end(line, caps_lock);
        kernel::terminal::write_char('\n');
        kernel::terminal::write_line("cancelled");
        line.clear();
        history.reset_browse();
        view.write_new_prompt_and_line(line, caps_lock);
        break;
    case 'e': {
        const kernel::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.move_to_end()) {
            view.redraw_change(line, caps_lock, kernel::EditorEditKind::CursorMove, before);
        }
        break;
    }
    case 'l':
        kernel::terminal::hide_cursor();
        kernel::terminal::clear();
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

void handle_key_event(const kernel::keyboard::KeyEvent& event, kernel::LineEditor& line,
                      kernel::shell::EditorView& view, bool& caps_lock, kernel::History& history) {
    if (!event.pressed) {
        return;
    }

    if (handle_control_shortcut(event, line, view, caps_lock, history)) {
        return;
    }

    switch (event.key) {
    case kernel::keyboard::Key::Character: {
        const kernel::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.insert(event.character)) {
            history.reset_browse();
            view.redraw_change(line, caps_lock, kernel::EditorEditKind::Insert, before);
        }
        break;
    }
    case kernel::keyboard::Key::Tab: {
        const kernel::EditorSnapshot before = view.snapshot(line, caps_lock);
        const size_t spaces = kernel::LineEditor::spaces_to_next_tab_stop(line.cursor());
        if (line.insert_spaces(spaces)) {
            history.reset_browse();
            view.redraw_change(line, caps_lock, kernel::EditorEditKind::Insert, before);
        }
        break;
    }
    case kernel::keyboard::Key::Backspace: {
        const kernel::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.backspace()) {
            history.reset_browse();
            view.redraw_change(line, caps_lock, kernel::EditorEditKind::Backspace, before);
        }
        break;
    }
    case kernel::keyboard::Key::Delete: {
        const kernel::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.delete_forward()) {
            history.reset_browse();
            view.redraw_change(line, caps_lock, kernel::EditorEditKind::DeleteForward, before);
        }
        break;
    }
    case kernel::keyboard::Key::UpArrow: {
        kernel::StringView command;
        view.redraw_history_result(history.previous(command), command, line, caps_lock);
        break;
    }
    case kernel::keyboard::Key::DownArrow: {
        kernel::StringView command;
        view.redraw_history_result(history.next(command), command, line, caps_lock);
        break;
    }
    case kernel::keyboard::Key::LeftArrow: {
        const kernel::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.move_left()) {
            view.redraw_change(line, caps_lock, kernel::EditorEditKind::CursorMove, before);
        }
        break;
    }
    case kernel::keyboard::Key::RightArrow: {
        const kernel::EditorSnapshot before = view.snapshot(line, caps_lock);
        if (line.move_right()) {
            view.redraw_change(line, caps_lock, kernel::EditorEditKind::CursorMove, before);
        }
        break;
    }
    case kernel::keyboard::Key::Enter:
        kernel::terminal::hide_cursor();
        view.move_to_line_end(line, caps_lock);
        kernel::terminal::write_char('\n');
        if (!line.empty()) {
            (void)history.push(line.view());
        }
        kernel::shell::execute_command(line.view());
        line.clear();
        history.reset_browse();
        view.write_new_prompt_and_line(line, caps_lock);
        break;
    case kernel::keyboard::Key::CapsLock: {
        const kernel::EditorSnapshot before = view.snapshot(line, caps_lock);
        caps_lock = event.caps_lock;
        view.redraw_change(line, caps_lock, kernel::EditorEditKind::PromptChange, before);
        break;
    }
    default:
        break;
    }
}

void handle_mouse_move_event(const kernel::input::MouseMoveEvent& event) {
    if (!event.x_overflow && !event.y_overflow) {
        kernel::mouse_cursor::move_by(event.delta_x, event.delta_y);
    }
}

void handle_input_event(const kernel::input::Event& event, kernel::LineEditor& line,
                        kernel::shell::EditorView& view, bool& caps_lock,
                        kernel::History& history) {
    switch (event.kind) {
    case kernel::input::EventKind::Key:
        handle_key_event(event.key, line, view, caps_lock, history);
        break;
    case kernel::input::EventKind::MouseMove:
        handle_mouse_move_event(event.mouse_move);
        break;
    case kernel::input::EventKind::None:
        break;
    }
}

} // namespace

namespace kernel::shell {

[[noreturn]] void run() {
    LineEditor line;
    History history;
    EditorView view;
    bool caps_lock = false;

    terminal::write_line("");
    terminal::write_line("interactive input ready");
    execute_command("help");
    view.write_new_prompt_and_line(line, caps_lock);
    serial::write_line("os-lab: interactive terminal ready");

    while (true) {
        input::Event event;
        if (input::poll(event)) {
            handle_input_event(event, line, view, caps_lock, history);
        } else {
            asm volatile("pause");
        }
    }
}

} // namespace kernel::shell
