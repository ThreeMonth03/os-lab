#include "shell_command_executor.hpp"

#include <stdint.h>

#include "kernel/halt.hpp"
#include "kernel/input/input.hpp"
#include "kernel/memory/memory.hpp"
#include "kernel/serial.hpp"
#include "kernel/shell/shell_command.hpp"
#include "kernel/terminal.hpp"

namespace {

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

void write_stat(kernel::StringView name, uint64_t value) {
    kernel::terminal::write_string("  ");
    kernel::terminal::write_string(name);
    kernel::terminal::write_string(": ");
    kernel::terminal::write_decimal(value);
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

} // namespace

namespace kernel::shell {

void execute_command(StringView command) {
    const ShellCommand parsed = parse_shell_command(command);

    switch (parsed.kind) {
    case ShellCommandKind::Empty:
        return;
    case ShellCommandKind::Help:
        write_help();
        break;
    case ShellCommandKind::Clear:
        terminal::clear();
        break;
    case ShellCommandKind::About:
        write_about();
        break;
    case ShellCommandKind::Input:
        write_input_stats();
        break;
    case ShellCommandKind::Mem:
        write_memory_stats();
        break;
    case ShellCommandKind::Halt:
        terminal::write_line("halting");
        serial::write_line("os-lab: halt command requested");
        halt_forever();
    case ShellCommandKind::Unknown:
        terminal::write_string("unknown command: ");
        terminal::write_line(parsed.text);
        break;
    }
}

} // namespace kernel::shell
