#include "kernel/fixed_vector.hpp"
#include "kernel/halt.hpp"
#include "kernel/keyboard.hpp"
#include "kernel/limine_support.hpp"
#include "kernel/serial.hpp"
#include "kernel/span.hpp"
#include "kernel/string_view.hpp"
#include "kernel/terminal.hpp"

namespace {

static_assert(kernel::StringView("os-lab").size() == 6);
static_assert(kernel::StringView("os-lab").starts_with("os"));

constexpr char kUtilitySmokeText[] = "kernel";
static_assert(kernel::Span<const char>(kUtilitySmokeText).size() == sizeof(kUtilitySmokeText));

constexpr kernel::StringView kPrompt = "> ";
constexpr size_t kLineCapacity = 80;
using LineBuffer = kernel::FixedVector<char, kLineCapacity>;

const char* firmware_name(uint64_t firmware_type) {
    switch (firmware_type) {
    case LIMINE_FIRMWARE_TYPE_X86BIOS:
        return "x86 BIOS";
    case LIMINE_FIRMWARE_TYPE_EFI32:
        return "UEFI 32-bit";
    case LIMINE_FIRMWARE_TYPE_EFI64:
        return "UEFI 64-bit";
    case LIMINE_FIRMWARE_TYPE_SBI:
        return "SBI";
    default:
        return "unknown";
    }
}

void run_utility_smoke() {
    kernel::FixedVector<kernel::StringView, 3> labels;
    const bool ok = labels.push_back("string_view") && labels.push_back("span") &&
                    labels.push_back("fixed_vector") && labels.full();
    const kernel::Span<const kernel::StringView> label_span(labels.data(), labels.size());

    if (!ok || label_span.size() != labels.capacity()) {
        kernel::serial::write_line("os-lab: no-heap utility smoke failed");
        return;
    }

    kernel::serial::write_line("os-lab: no-heap utilities ready");
}

void write_prompt() { kernel::terminal::write_string(kPrompt); }

kernel::StringView line_view(const LineBuffer& line) { return {line.data(), line.size()}; }

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

void handle_line(const LineBuffer& line) {
    if (line.empty()) {
        return;
    }

    const kernel::StringView command = line_view(line);

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

void handle_key_event(const kernel::keyboard::KeyEvent& event, LineBuffer& line) {
    if (!event.pressed) {
        return;
    }

    switch (event.key) {
    case kernel::keyboard::Key::Character:
        if (line.push_back(event.character)) {
            kernel::terminal::write_char(event.character);
        }
        break;
    case kernel::keyboard::Key::Backspace:
        if (!line.empty()) {
            line.pop_back();
            kernel::terminal::write_char('\b');
        }
        break;
    case kernel::keyboard::Key::Enter:
        kernel::terminal::write_char('\n');
        handle_line(line);
        line.clear();
        write_prompt();
        break;
    default:
        break;
    }
}

[[noreturn]] void run_interactive_terminal() {
    LineBuffer line;

    kernel::terminal::write_line("");
    kernel::terminal::write_line("interactive input ready");
    write_help();
    write_prompt();
    kernel::serial::write_line("os-lab: interactive terminal ready");

    while (true) {
        kernel::keyboard::KeyEvent event;
        if (kernel::keyboard::poll_key(event)) {
            handle_key_event(event, line);
        } else {
            asm volatile("pause");
        }
    }
}

} // namespace

extern "C" [[noreturn]] void kernel_main() {
    kernel::serial::write_line("os-lab: kernel main entered");
    const bool terminal_ready = kernel::terminal::init();
    if (terminal_ready) {
        kernel::terminal::write_line("os-lab terminal");
        kernel::terminal::write_line("filesystem unavailable");
        kernel::terminal::write_line("serial debug enabled");
        kernel::terminal::write_line("");
    }

    run_utility_smoke();

    if (const auto* info = kernel::boot::bootloader_info(); info != nullptr) {
        kernel::serial::write_string("os-lab: bootloader = ");
        kernel::serial::write_string(info->name);
        kernel::serial::write_string(" ");
        kernel::serial::write_line(info->version);

        kernel::terminal::write_string("bootloader = ");
        kernel::terminal::write_string(info->name);
        kernel::terminal::write_string(" ");
        kernel::terminal::write_line(info->version);
    }

    kernel::serial::write_string("os-lab: firmware = ");
    kernel::serial::write_line(firmware_name(kernel::boot::firmware_type()));
    kernel::terminal::write_string("firmware = ");
    kernel::terminal::write_line(firmware_name(kernel::boot::firmware_type()));

    kernel::serial::write_string("os-lab: loaded base revision = ");
    kernel::serial::write_decimal(kernel::boot::loaded_base_revision());
    kernel::serial::write_string("\n");

    kernel::serial::write_line(terminal_ready ? "os-lab: framebuffer terminal active"
                                              : "os-lab: framebuffer terminal unavailable");
    run_interactive_terminal();
}
