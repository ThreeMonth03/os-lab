#include "kernel/shell.hpp"

#include <stdint.h>

#include "kernel/halt.hpp"
#include "kernel/keyboard.hpp"
#include "kernel/line_editor.hpp"
#include "kernel/serial.hpp"
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
    kernel::terminal::write_line("  halt  - stop the cpu");
}

void write_about() {
    kernel::terminal::write_line("os-lab early shell");
    kernel::terminal::write_line("freestanding c++23 kernel");
    kernel::terminal::write_line("no filesystem or heap yet");
}

void handle_line(kernel::StringView command) {
    if (command.empty()) {
        return;
    }

    if (command == "help") {
        write_help();
    } else if (command == "clear") {
        kernel::terminal::clear();
    } else if (command == "about") {
        write_about();
    } else if (command == "halt") {
        kernel::terminal::write_line("halting");
        kernel::serial::write_line("os-lab: halt command requested");
        kernel::halt_forever();
    } else {
        kernel::terminal::write_string("unknown command: ");
        kernel::terminal::write_line(command);
    }
}

bool handle_control_shortcut(const kernel::keyboard::KeyEvent& event, kernel::LineEditor& line,
                             LinePosition& position, bool caps_lock) {
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
        redraw_line(line, position);
        break;
    default:
        break;
    }

    return true;
}

void handle_key_event(const kernel::keyboard::KeyEvent& event, kernel::LineEditor& line,
                      LinePosition& position, bool& caps_lock) {
    if (!event.pressed) {
        return;
    }

    if (handle_control_shortcut(event, line, position, caps_lock)) {
        return;
    }

    switch (event.key) {
    case kernel::keyboard::Key::Character:
        if (line.insert(event.character)) {
            redraw_line(line, position);
        }
        break;
    case kernel::keyboard::Key::Backspace:
        if (line.backspace()) {
            redraw_line(line, position);
        }
        break;
    case kernel::keyboard::Key::Delete:
        if (line.delete_forward()) {
            redraw_line(line, position);
        }
        break;
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
        handle_line(line.view());
        line.clear();
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

} // namespace

namespace kernel::shell {

[[noreturn]] void run() {
    LineEditor line;
    LinePosition position;
    bool caps_lock = false;

    terminal::write_line("");
    terminal::write_line("interactive input ready");
    write_help();
    write_new_prompt_and_line(line, position, caps_lock);
    serial::write_line("os-lab: interactive terminal ready");

    while (true) {
        keyboard::KeyEvent event;
        if (keyboard::poll_key(event)) {
            handle_key_event(event, line, position, caps_lock);
        } else {
            asm volatile("pause");
        }
    }
}

} // namespace kernel::shell
