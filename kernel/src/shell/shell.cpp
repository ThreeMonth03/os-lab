#include "kernel/shell/shell.hpp"

#include <stddef.h>
#include <stdint.h>

#include "kernel/text/editor_dirty_range.hpp"
#include "kernel/text/editor_view_layout.hpp"
#include "kernel/halt.hpp"
#include "kernel/text/history.hpp"
#include "kernel/input/input.hpp"
#include "kernel/input/keyboard.hpp"
#include "kernel/text/line_editor.hpp"
#include "kernel/memory/memory.hpp"
#include "kernel/mouse_cursor.hpp"
#include "kernel/serial.hpp"
#include "kernel/shell/shell_command.hpp"
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
    uint64_t rendered_rows = 1;
};

kernel::StringView prompt_for_caps(bool caps_lock) {
    return caps_lock ? kCapsPrompt : kDefaultPrompt;
}

kernel::EditorSnapshot editor_snapshot(const kernel::LineEditor& line, bool caps_lock) {
    return {line.cursor(), line.size(), prompt_for_caps(caps_lock).size()};
}

kernel::EditorViewLayout editor_layout(const LinePosition& position, bool caps_lock) {
    return {kernel::terminal::columns(), position.prompt_column, prompt_for_caps(caps_lock).size()};
}

uint64_t max_u64(uint64_t left, uint64_t right) { return left > right ? left : right; }

void scroll_to_fit(LinePosition& position, uint64_t rows_needed) {
    const uint64_t rows = kernel::terminal::rows();
    if (rows == 0) {
        return;
    }

    const uint64_t target_prompt_row = rows_needed >= rows ? 0 : rows - rows_needed;
    if (position.prompt_row <= target_prompt_row) {
        return;
    }

    const uint64_t scroll_count = position.prompt_row - target_prompt_row;
    for (uint64_t count = 0; count < scroll_count; ++count) {
        kernel::terminal::set_cursor(0, rows - 1);
        kernel::terminal::write_char('\n');
    }

    position.prompt_row = target_prompt_row;
}

void clear_rendered_area(const LinePosition& position, uint64_t rows_to_clear) {
    const uint64_t rows = kernel::terminal::rows();
    for (uint64_t row = 0; row < rows_to_clear && position.prompt_row + row < rows; ++row) {
        const uint64_t column = row == 0 ? position.prompt_column : 0;
        kernel::terminal::clear_row_from(column, position.prompt_row + row);
    }
}

void set_editor_cursor(const LinePosition& position, bool caps_lock, size_t index) {
    const kernel::EditorViewCell cell = editor_layout(position, caps_lock).position_for(index);
    kernel::terminal::set_cursor(cell.column, position.prompt_row + cell.row);
}

void draw_text_range(kernel::StringView text, const LinePosition& position,
                     const kernel::EditorViewLayout& layout, size_t start, size_t end) {
    for (size_t index = start; index < end && index < text.size(); ++index) {
        const kernel::EditorViewCell cell = layout.position_for(index);
        kernel::terminal::draw_char_at(cell.column, position.prompt_row + cell.row, text[index]);
    }
}

void clear_text_range(const LinePosition& position, const kernel::EditorViewLayout& layout,
                      size_t start, size_t end) {
    for (size_t index = start; index < end; ++index) {
        const kernel::EditorViewCell cell = layout.position_for(index);
        kernel::terminal::clear_cell_at(cell.column, position.prompt_row + cell.row);
    }
}

void redraw_prompt_and_line(const kernel::LineEditor& line, LinePosition& position,
                            bool caps_lock) {
    kernel::terminal::hide_cursor();

    const kernel::StringView prompt = prompt_for_caps(caps_lock);
    const kernel::EditorViewLayout layout = {kernel::terminal::columns(), position.prompt_column,
                                             prompt.size()};
    const uint64_t rows_needed = layout.visual_rows(line.view().size());

    scroll_to_fit(position, rows_needed);
    clear_rendered_area(position, max_u64(position.rendered_rows, rows_needed));

    kernel::terminal::set_cursor(position.prompt_column, position.prompt_row);
    kernel::terminal::write_string(prompt);
    position.input_column = kernel::terminal::cursor_column();
    position.input_row = kernel::terminal::cursor_row();
    kernel::terminal::write_string(line.view());
    position.rendered_rows = rows_needed;

    set_editor_cursor(position, caps_lock, line.cursor());
    kernel::terminal::show_cursor();
}

void redraw_dirty_range(const kernel::LineEditor& line, LinePosition& position, bool caps_lock,
                        kernel::EditorDirtyRange dirty) {
    switch (dirty.kind) {
    case kernel::EditorDirtyKind::None:
        return;
    case kernel::EditorDirtyKind::CursorOnly:
        set_editor_cursor(position, caps_lock, line.cursor());
        kernel::terminal::show_cursor();
        return;
    case kernel::EditorDirtyKind::Full:
        redraw_prompt_and_line(line, position, caps_lock);
        return;
    case kernel::EditorDirtyKind::Partial:
        break;
    }

    kernel::terminal::hide_cursor();

    const kernel::EditorViewLayout layout = editor_layout(position, caps_lock);
    const uint64_t rows_needed = layout.visual_rows(line.view().size());

    scroll_to_fit(position, rows_needed);
    draw_text_range(line.view(), position, layout, dirty.start, dirty.new_end);
    clear_text_range(position, layout, dirty.new_end, dirty.old_end);
    position.rendered_rows = rows_needed;

    set_editor_cursor(position, caps_lock, line.cursor());
    kernel::terminal::show_cursor();
}

void redraw_editor_change(const kernel::LineEditor& line, LinePosition& position, bool caps_lock,
                          kernel::EditorEditKind edit, kernel::EditorSnapshot before) {
    const kernel::EditorSnapshot after = editor_snapshot(line, caps_lock);
    redraw_dirty_range(line, position, caps_lock, kernel::editor_dirty_range(edit, before, after));
}

void write_new_prompt_and_line(const kernel::LineEditor& line, LinePosition& position,
                               bool caps_lock) {
    position.prompt_column = kernel::terminal::cursor_column();
    position.prompt_row = kernel::terminal::cursor_row();
    position.rendered_rows = 1;
    redraw_prompt_and_line(line, position, caps_lock);
}

void move_to_line_end(const kernel::LineEditor& line, const LinePosition& position,
                      bool caps_lock) {
    set_editor_cursor(position, caps_lock, line.view().size());
}

void redraw_history_result(kernel::HistoryResult result, kernel::StringView command,
                           kernel::LineEditor& line, LinePosition& position, bool caps_lock) {
    switch (result) {
    case kernel::HistoryResult::Command:
        if (line.replace(command)) {
            redraw_prompt_and_line(line, position, caps_lock);
        }
        break;
    case kernel::HistoryResult::Blank:
        line.clear();
        redraw_prompt_and_line(line, position, caps_lock);
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
    kernel::terminal::write_line("  mem   - show memory stats");
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

void write_memory_stats() {
    const kernel::memory::Stats stats = kernel::memory::stats();
    if (!stats.initialized) {
        kernel::terminal::write_line("memory stats unavailable");
        return;
    }

    kernel::terminal::write_line("memory stats:");
    write_stat("regions", stats.map.region_count);
    write_stat("usable KiB", stats.map.usable_bytes / 1024);
    write_stat("bootloader reclaimable KiB", stats.map.bootloader_reclaimable_bytes / 1024);
    write_stat("framebuffer KiB", stats.map.framebuffer_bytes / 1024);
    write_stat("frame size", kernel::memory::kFrameSize);
    write_stat("total frames", stats.frames.total_frames);
    write_stat("allocated frames", stats.frames.allocated_frames);
    write_stat("remaining frames", stats.frames.remaining_frames);
    kernel::terminal::write_string("  truncated: ");
    kernel::terminal::write_line(stats.truncated ? "yes" : "no");
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
    case kernel::ShellCommandKind::Mem:
        write_memory_stats();
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
    case 'a': {
        const kernel::EditorSnapshot before = editor_snapshot(line, caps_lock);
        if (line.move_to_start()) {
            redraw_editor_change(line, position, caps_lock, kernel::EditorEditKind::CursorMove,
                                 before);
        }
        break;
    }
    case 'c':
        kernel::terminal::hide_cursor();
        move_to_line_end(line, position, caps_lock);
        kernel::terminal::write_char('\n');
        kernel::terminal::write_line("cancelled");
        line.clear();
        history.reset_browse();
        write_new_prompt_and_line(line, position, caps_lock);
        break;
    case 'e': {
        const kernel::EditorSnapshot before = editor_snapshot(line, caps_lock);
        if (line.move_to_end()) {
            redraw_editor_change(line, position, caps_lock, kernel::EditorEditKind::CursorMove,
                                 before);
        }
        break;
    }
    case 'l':
        kernel::terminal::hide_cursor();
        kernel::terminal::clear();
        write_new_prompt_and_line(line, position, caps_lock);
        break;
    case 'u':
        line.clear();
        history.reset_browse();
        redraw_prompt_and_line(line, position, caps_lock);
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
    case kernel::keyboard::Key::Character: {
        const kernel::EditorSnapshot before = editor_snapshot(line, caps_lock);
        if (line.insert(event.character)) {
            history.reset_browse();
            redraw_editor_change(line, position, caps_lock, kernel::EditorEditKind::Insert, before);
        }
        break;
    }
    case kernel::keyboard::Key::Tab: {
        const kernel::EditorSnapshot before = editor_snapshot(line, caps_lock);
        const size_t spaces = kernel::LineEditor::spaces_to_next_tab_stop(line.cursor());
        if (line.insert_spaces(spaces)) {
            history.reset_browse();
            redraw_editor_change(line, position, caps_lock, kernel::EditorEditKind::Insert, before);
        }
        break;
    }
    case kernel::keyboard::Key::Backspace: {
        const kernel::EditorSnapshot before = editor_snapshot(line, caps_lock);
        if (line.backspace()) {
            history.reset_browse();
            redraw_editor_change(line, position, caps_lock, kernel::EditorEditKind::Backspace,
                                 before);
        }
        break;
    }
    case kernel::keyboard::Key::Delete: {
        const kernel::EditorSnapshot before = editor_snapshot(line, caps_lock);
        if (line.delete_forward()) {
            history.reset_browse();
            redraw_editor_change(line, position, caps_lock, kernel::EditorEditKind::DeleteForward,
                                 before);
        }
        break;
    }
    case kernel::keyboard::Key::UpArrow: {
        kernel::StringView command;
        redraw_history_result(history.previous(command), command, line, position, caps_lock);
        break;
    }
    case kernel::keyboard::Key::DownArrow: {
        kernel::StringView command;
        redraw_history_result(history.next(command), command, line, position, caps_lock);
        break;
    }
    case kernel::keyboard::Key::LeftArrow: {
        const kernel::EditorSnapshot before = editor_snapshot(line, caps_lock);
        if (line.move_left()) {
            redraw_editor_change(line, position, caps_lock, kernel::EditorEditKind::CursorMove,
                                 before);
        }
        break;
    }
    case kernel::keyboard::Key::RightArrow: {
        const kernel::EditorSnapshot before = editor_snapshot(line, caps_lock);
        if (line.move_right()) {
            redraw_editor_change(line, position, caps_lock, kernel::EditorEditKind::CursorMove,
                                 before);
        }
        break;
    }
    case kernel::keyboard::Key::Enter:
        kernel::terminal::hide_cursor();
        move_to_line_end(line, position, caps_lock);
        kernel::terminal::write_char('\n');
        if (!line.empty()) {
            (void)history.push(line.view());
        }
        handle_line(line.view());
        line.clear();
        history.reset_browse();
        write_new_prompt_and_line(line, position, caps_lock);
        break;
    case kernel::keyboard::Key::CapsLock: {
        const kernel::EditorSnapshot before = editor_snapshot(line, caps_lock);
        caps_lock = event.caps_lock;
        redraw_editor_change(line, position, caps_lock, kernel::EditorEditKind::PromptChange,
                             before);
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
