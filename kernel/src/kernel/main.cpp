#include "kernel/framebuffer.hpp"
#include "kernel/halt.hpp"
#include "kernel/limine_support.hpp"
#include "kernel/serial.hpp"

namespace {

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

} // namespace

extern "C" [[noreturn]] void kernel_main() {
    kernel::serial::write_line("os-lab: kernel main entered");

    if (const auto* info = kernel::boot::bootloader_info(); info != nullptr) {
        kernel::serial::write_string("os-lab: bootloader = ");
        kernel::serial::write_string(info->name);
        kernel::serial::write_string(" ");
        kernel::serial::write_line(info->version);
    }

    kernel::serial::write_string("os-lab: firmware = ");
    kernel::serial::write_line(firmware_name(kernel::boot::firmware_type()));

    kernel::serial::write_string("os-lab: loaded base revision = ");
    kernel::serial::write_decimal(kernel::boot::loaded_base_revision());
    kernel::serial::write_string("\n");

    kernel::framebuffer::paint_boot_splash();
    kernel::serial::write_line("os-lab: framebuffer splash drawn if available");
    kernel::serial::write_line("os-lab: system halted");

    kernel::halt_forever();
}
