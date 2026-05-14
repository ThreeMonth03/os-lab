#include "kernel/arch/x86_64/exception_smoke.hpp"
#include "kernel/fixed_vector.hpp"
#include "kernel/limine_support.hpp"
#include "kernel/mouse.hpp"
#include "kernel/mouse_cursor.hpp"
#include "kernel/serial.hpp"
#include "kernel/shell.hpp"
#include "kernel/span.hpp"
#include "kernel/string_view.hpp"
#include "kernel/terminal.hpp"

namespace {

static_assert(kernel::StringView("os-lab").size() == 6);
static_assert(kernel::StringView("os-lab").starts_with("os"));

constexpr char kUtilitySmokeText[] = "kernel";
static_assert(kernel::Span<const char>(kUtilitySmokeText).size() == sizeof(kUtilitySmokeText));

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

    kernel::arch::x86_64::run_exception_smoke();

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

    const bool mouse_ready = kernel::mouse::init();
    const bool mouse_cursor_ready = mouse_ready && kernel::mouse_cursor::init();
    if (mouse_cursor_ready && mouse_ready) {
        kernel::mouse_cursor::show();
        kernel::serial::write_line("os-lab: ps/2 mouse cursor active");
    } else {
        kernel::serial::write_line("os-lab: ps/2 mouse cursor unavailable");
    }

    kernel::shell::run();
}
