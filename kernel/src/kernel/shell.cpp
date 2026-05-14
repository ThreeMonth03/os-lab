#include "kernel/shell.hpp"

#include "kernel/halt.hpp"
#include "kernel/keyboard.hpp"
#include "kernel/line_editor.hpp"
#include "kernel/serial.hpp"
#include "kernel/string_view.hpp"
#include "kernel/terminal.hpp"

namespace {

constexpr kernel::StringView kPrompt = "> ";

void write_prompt() { kernel::terminal::write_string(kPrompt); }

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

void handle_key_event(const kernel::keyboard::KeyEvent& event, kernel::LineEditor& line) {
    if (!event.pressed) {
        return;
    }

    switch (event.key) {
    case kernel::keyboard::Key::Character:
        if (line.insert(event.character)) {
            kernel::terminal::write_char(event.character);
        }
        break;
    case kernel::keyboard::Key::Backspace:
        if (line.backspace()) {
            kernel::terminal::write_char('\b');
        }
        break;
    case kernel::keyboard::Key::Enter:
        kernel::terminal::write_char('\n');
        handle_line(line.view());
        line.clear();
        write_prompt();
        break;
    default:
        break;
    }
}

} // namespace

namespace kernel::shell {

[[noreturn]] void run() {
    LineEditor line;

    terminal::write_line("");
    terminal::write_line("interactive input ready");
    write_help();
    write_prompt();
    serial::write_line("os-lab: interactive terminal ready");

    while (true) {
        keyboard::KeyEvent event;
        if (keyboard::poll_key(event)) {
            handle_key_event(event, line);
        } else {
            asm volatile("pause");
        }
    }
}

} // namespace kernel::shell
