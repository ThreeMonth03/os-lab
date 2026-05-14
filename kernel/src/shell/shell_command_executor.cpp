#include "shell_command_executor.hpp"

#include <stdint.h>

#include "kernel/core/halt.hpp"
#include "kernel/input/input.hpp"
#include "kernel/memory/memory.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/shell/shell_command.hpp"
#include "kernel/console/terminal.hpp"

namespace {

namespace serial = kernel::drivers::serial;
namespace terminal = kernel::console::terminal;

void write_help() {
    terminal::write_line("commands:");
    terminal::write_line("  help  - show this list");
    terminal::write_line("  clear - clear the screen");
    terminal::write_line("  about - show kernel info");
    terminal::write_line("  input - show input stats");
    terminal::write_line("  mem   - show memory stats");
    terminal::write_line("  halt  - stop the cpu");
}

void write_about() {
    terminal::write_line("os-lab early shell");
    terminal::write_line("freestanding c++23 kernel");
    terminal::write_line("no filesystem or heap yet");
}

void write_stat(kernel::StringView name, uint64_t value) {
    terminal::write_string("  ");
    terminal::write_string(name);
    terminal::write_string(": ");
    terminal::write_decimal(value);
    terminal::write_char('\n');
}

void write_input_stats() {
    const kernel::input::Stats stats = kernel::input::stats();

    terminal::write_line("input stats:");
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
        terminal::write_line("memory stats unavailable");
        return;
    }

    terminal::write_line("memory stats:");
    write_stat("regions", stats.map.region_count);
    write_stat("usable KiB", stats.map.usable_bytes / 1024);
    write_stat("bootloader reclaimable KiB", stats.map.bootloader_reclaimable_bytes / 1024);
    write_stat("framebuffer KiB", stats.map.framebuffer_bytes / 1024);
    write_stat("frame size", kernel::memory::kFrameSize);
    write_stat("total frames", stats.frames.total_frames);
    write_stat("allocated frames", stats.frames.allocated_frames);
    write_stat("remaining frames", stats.frames.remaining_frames);
    terminal::write_string("  truncated: ");
    terminal::write_line(stats.truncated ? "yes" : "no");
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
