#include "kernel/shell.hpp"

#include <stddef.h>
#include <stdint.h>

#include "kernel/halt.hpp"
#include "kernel/history.hpp"
#include "kernel/input.hpp"
#include "kernel/keyboard.hpp"
#include "kernel/line_editor.hpp"
#include "kernel/mouse_cursor.hpp"
#include "kernel/serial.hpp"
#include "kernel/shell_command.hpp"
#include "kernel/string_view.hpp"
#include "kernel/terminal.hpp"

namespace {

constexpr kernel::StringView kDefaultPrompt = "> ";
constexpr kernel::StringView kCapsPrompt = "[CAPS] > ";

struct LinePosition {
    uint64_t prompt_column = 0;
    uint64_t prompt_row = 0;
    uint64_t input_column = 0;
    uint64_t input_row = 0;
};

kernel::StringView prompt_for_caps(bool caps_lock) {
    return caps_lock ? kCapsPrompt : kDefaultPrompt;
}

size_t line_capacity() {
    const uint64_t columns = kernel::terminal::columns();
    if (columns <= kCapsPrompt.size() + 1) {
        return 0;
    }

    uint64_t capacity = columns - kCapsPrompt.size() - 1;
    if (capacity > kernel::LineEditor::capacity) {
        capacity = kernel::LineEditor::capacity;
    }

    return static_cast<size_t>(capacity);
}

void write_prompt(LinePosition& position, bool caps_lock) {
    kernel::terminal::hide_cursor();
    position.prompt_column = kernel::terminal::cursor_column();
    position.prompt_row = kernel::terminal::cursor_row();
    kernel::terminal::write_string(prompt_for_caps(caps_lock));
    position.input_column = kernel::terminal::cursor_column();
    position.input_row = kernel::terminal::cursor_row();
}

void redraw_line(const kernel::LineEditor& line, const LinePosition& position) {
    kernel::terminal::hide_cursor();
    kernel::terminal::set_cursor(position.input_column, position.input_row);
    kernel::terminal::write_string(line.view());
    kernel::terminal::clear_row_from(position.input_column + line.view().size(),
                                     position.input_row);
    kernel::terminal::set_cursor(position.input_column + line.cursor(), position.input_row);
    kernel::terminal::show_cursor();
}

void write_new_prompt_and_line(const kernel::LineEditor& line, LinePosition& position,
                               bool caps_lock) {
    write_prompt(position, caps_lock);
    redraw_line(line, position);
}

void redraw_prompt_and_line(const kernel::LineEditor& line, LinePosition& position,
                            bool caps_lock) {
    kernel::terminal::hide_cursor();
    kernel::terminal::set_cursor(position.prompt_column, position.prompt_row);
    kernel::terminal::clear_row_from(position.prompt_column, position.prompt_row);
    write_prompt(position, caps_lock);
    redraw_line(line, position);
}

void move_to_line_end(const kernel::LineEditor& line, const LinePosition& position) {
    kernel::terminal::set_cursor(position.input_column + line.view().size(), position.input_row);
}

void redraw_history_result(kernel::HistoryResult result, kernel::StringView command,
                           kernel::LineEditor& line, const LinePosition& position) {
    switch (result) {
    case kernel::HistoryResult::Command:
        if (command.size() <= line_capacity() && line.replace(command)) {
            redraw_line(line, position);
        }
        break;
    case kernel::HistoryResult::Blank:
        line.clear();
        redraw_line(line, position);
        break;
    case kernel::HistoryResult::None:
        break;
    }
}

char lowercase(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }

    return value;
}

void write_help() {
    kernel::terminal::write_line("commands:");
    kernel::terminal::write_line("  help  - show this list");
    kernel::terminal::write_line("  clear - clear the screen");
    kernel::terminal::write_line("  about - show kernel info");
    kernel::terminal::write_line("  input - show input stats");
    kernel::terminal::write_line("  halt  - stop the cpu");
}

void write_about() {
    kernel::terminal::write_line("os-lab early shell");
    kernel::terminal::write_line("freestanding c++23 kernel");
    kernel::terminal::write_line("no filesystem or heap yet");
}

void write_decimal(uint64_t value) {
    char buffer[21] = {};
    size_t index = 0;

    if (value == 0) {
        kernel::terminal::write_char('0');
        return;
    }

    while (value > 0) {
        buffer[index++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        kernel::terminal::write_char(buffer[--index]);
    }
}

void write_stat(kernel::StringView name, uint64_t value) {
    kernel::terminal::write_string("  ");
    kernel::terminal::write_string(name);
    kernel::terminal::write_string(": ");
    write_decimal(value);
    kernel::terminal::write_char('\n');
}

void write_input_stats() {
    const kernel::input::Stats stats = kernel::input::stats();

    kernel::terminal::write_line("input stats:");
    write_stat("key events", stats.key_events);
    write_stat("mouse move events", stats.mouse_move_events);
    write_stat("dropped events", stats.dropped_events);
    write_stat("queued events", stats.queued_events);
    write_stat("queue available", stats.queue_available);
    write_stat("queue capacity", stats.queue_capacity);
}

void handle_line(kernel::StringView command) {
    const kernel::ShellCommand parsed = kernel::parse_shell_command(command);

    switch (parsed.kind) {
    case kernel::ShellCommandKind::Empty:
        return;
    case kernel::ShellCommandKind::Help:
        write_help();
        break;
    case kernel::ShellCommandKind::Clear:
        kernel::terminal::clear();
        break;
    case kernel::ShellCommandKind::About:
        write_about();
        break;
    case kernel::ShellCommandKind::Input:
        write_input_stats();
        break;
    case kernel::ShellCommandKind::Halt:
        kernel::terminal::write_line("halting");
        kernel::serial::write_line("os-lab: halt command requested");
        kernel::halt_forever();
    case kernel::ShellCommandKind::Unknown:
        kernel::terminal::write_string("unknown command: ");
        kernel::terminal::write_line(parsed.text);
        break;
    }
}

bool handle_control_shortcut(const kernel::keyboard::KeyEvent& event, kernel::LineEditor& line,
                             LinePosition& position, bool caps_lock, kernel::History& history) {
    if (!event.control || event.key != kernel::keyboard::Key::Character) {
        return false;
    }

    switch (lowercase(event.character)) {
    case 'a':
        if (line.move_to_start()) {
            redraw_line(line, position);
        }
        break;
    case 'c':
        kernel::terminal::hide_cursor();
        move_to_line_end(line, position);
        kernel::terminal::write_char('\n');
        kernel::terminal::write_line("cancelled");
        line.clear();
        history.reset_browse();
        write_new_prompt_and_line(line, position, caps_lock);
        break;
    case 'e':
        if (line.move_to_end()) {
            redraw_line(line, position);
        }
        break;
    case 'l':
        kernel::terminal::hide_cursor();
        kernel::terminal::clear();
        write_new_prompt_and_line(line, position, caps_lock);
        break;
    case 'u':
        line.clear();
        history.reset_browse();
        redraw_line(line, position);
        break;
    default:
        break;
    }

    return true;
}

void handle_key_event(const kernel::keyboard::KeyEvent& event, kernel::LineEditor& line,
                      LinePosition& position, bool& caps_lock, kernel::History& history) {
    if (!event.pressed) {
        return;
    }

    if (handle_control_shortcut(event, line, position, caps_lock, history)) {
        return;
    }

    switch (event.key) {
    case kernel::keyboard::Key::Character:
        if (line.size() < line_capacity() && line.insert(event.character)) {
            history.reset_browse();
            redraw_line(line, position);
        }
        break;
    case kernel::keyboard::Key::Backspace:
        if (line.backspace()) {
            history.reset_browse();
            redraw_line(line, position);
        }
        break;
    case kernel::keyboard::Key::Delete:
        if (line.delete_forward()) {
            history.reset_browse();
            redraw_line(line, position);
        }
        break;
    case kernel::keyboard::Key::UpArrow: {
        kernel::StringView command;
        redraw_history_result(history.previous(command), command, line, position);
        break;
    }
    case kernel::keyboard::Key::DownArrow: {
        kernel::StringView command;
        redraw_history_result(history.next(command), command, line, position);
        break;
    }
    case kernel::keyboard::Key::LeftArrow:
        if (line.move_left()) {
            redraw_line(line, position);
        }
        break;
    case kernel::keyboard::Key::RightArrow:
        if (line.move_right()) {
            redraw_line(line, position);
        }
        break;
    case kernel::keyboard::Key::Enter:
        kernel::terminal::hide_cursor();
        kernel::terminal::set_cursor(position.input_column + line.view().size(),
                                     position.input_row);
        kernel::terminal::write_char('\n');
        if (!line.empty()) {
            (void)history.push(line.view());
        }
        handle_line(line.view());
        line.clear();
        history.reset_browse();
        write_new_prompt_and_line(line, position, caps_lock);
        break;
    case kernel::keyboard::Key::CapsLock:
        caps_lock = event.caps_lock;
        redraw_prompt_and_line(line, position, caps_lock);
        break;
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
                        LinePosition& position, bool& caps_lock, kernel::History& history) {
    switch (event.kind) {
    case kernel::input::EventKind::Key:
        handle_key_event(event.key, line, position, caps_lock, history);
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
    LinePosition position;
    bool caps_lock = false;

    terminal::write_line("");
    terminal::write_line("interactive input ready");
    write_help();
    write_new_prompt_and_line(line, position, caps_lock);
    serial::write_line("os-lab: interactive terminal ready");

    while (true) {
        input::Event event;
        if (input::poll(event)) {
            handle_input_event(event, line, position, caps_lock, history);
        } else {
            asm volatile("pause");
        }
    }
}

} // namespace kernel::shell
