#include "kernel/arch/x86_64/exception_smoke.hpp"
#include "kernel/arch/x86_64/irq.hpp"
#include "kernel/arch/x86_64/paging.hpp"
#include "kernel/base/fixed_vector.hpp"
#include "kernel/boot/limine_support.hpp"
#include "kernel/memory/memory.hpp"
#include "kernel/memory/heap.hpp"
#include "kernel/memory/slab.hpp"
#include "kernel/debug/heap_smoke.hpp"
#include "kernel/debug/paging_smoke.hpp"
#include "kernel/debug/slab_smoke.hpp"
#include "kernel/drivers/mouse.hpp"
#include "kernel/drivers/keyboard.hpp"
#include "kernel/display/mouse_cursor.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/shell/shell.hpp"
#include "kernel/base/span.hpp"
#include "kernel/base/string_view.hpp"
#include "kernel/console/terminal.hpp"
#include "kernel/time/timer.hpp"
#include "kernel/debug/timer_smoke.hpp"

namespace
{

static_assert(kernel::StringView("os-lab").size() == 6);
static_assert(kernel::StringView("os-lab").starts_with("os"));

constexpr char kUtilitySmokeText[] = "kernel";
static_assert(kernel::Span<const char>(kUtilitySmokeText).size() == sizeof(kUtilitySmokeText));

const char * firmware_name(uint64_t firmware_type)
{
    switch (firmware_type)
    {
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

void run_utility_smoke()
{
    kernel::FixedVector<kernel::StringView, 3> labels;
    const bool ok = labels.push_back("string_view") && labels.push_back("span") &&
                    labels.push_back("fixed_vector") && labels.full();
    const kernel::Span<const kernel::StringView> label_span(labels.data(), labels.size());

    if (!ok || label_span.size() != labels.capacity())
    {
        kernel::drivers::serial::write_line("os-lab: no-heap utility smoke failed");
        return;
    }

    kernel::drivers::serial::write_line("os-lab: no-heap utilities ready");
}

void write_memory_summary(bool ready)
{
    if (!ready)
    {
        kernel::drivers::serial::write_line("os-lab: memory map unavailable");
        kernel::console::terminal::write_line("memory map unavailable");
        return;
    }

    const kernel::memory::Stats stats = kernel::memory::stats();
    const uint64_t usable_kib = stats.map.usable_bytes / 1024;

    kernel::drivers::serial::write_string("os-lab: memory map regions = ");
    kernel::drivers::serial::write_decimal(stats.map.region_count);
    kernel::drivers::serial::write_string("\n");
    kernel::drivers::serial::write_string("os-lab: memory usable KiB = ");
    kernel::drivers::serial::write_decimal(usable_kib);
    kernel::drivers::serial::write_string("\n");
    kernel::drivers::serial::write_string("os-lab: frame allocator frames = ");
    kernel::drivers::serial::write_decimal(stats.frames.total_frames);
    kernel::drivers::serial::write_string("\n");
    if (stats.truncated)
    {
        kernel::drivers::serial::write_line("os-lab: memory map truncated");
    }

    kernel::console::terminal::write_string("memory usable = ");
    kernel::console::terminal::write_decimal(usable_kib);
    kernel::console::terminal::write_line(" KiB");
}

bool init_memory_and_paging()
{
    const bool memory_ready = kernel::memory::init();
    if (memory_ready)
    {
        if (!kernel::arch::x86_64::paging::init_foundation())
        {
            kernel::drivers::serial::write_line("os-lab: x86_64 paging foundation unavailable");
        }
    }
    return memory_ready;
}

void init_kernel_heap()
{
    if (kernel::memory::heap::init())
    {
        kernel::drivers::serial::write_line("os-lab: kernel heap ready");
    }
    else
    {
        kernel::drivers::serial::write_line("os-lab: kernel heap unavailable");
    }
}

void init_kernel_slab()
{
    if (kernel::memory::slab::init())
    {
        kernel::drivers::serial::write_line("os-lab: kernel slab cache ready");
    }
    else
    {
        kernel::drivers::serial::write_line("os-lab: kernel slab cache unavailable");
    }
}

void write_terminal_banner()
{
    kernel::console::terminal::write_line("os-lab terminal");
    kernel::console::terminal::write_line("filesystem unavailable");
    kernel::console::terminal::write_line("serial debug enabled");
    kernel::console::terminal::write_line("");
}

bool init_terminal()
{
    const bool ready = kernel::console::terminal::init();
    if (ready)
    {
        write_terminal_banner();
    }
    return ready;
}

void write_bootloader_info()
{
    const auto * info = kernel::boot::bootloader_info();
    if (info == nullptr)
    {
        return;
    }

    kernel::drivers::serial::write_string("os-lab: bootloader = ");
    kernel::drivers::serial::write_string(info->name);
    kernel::drivers::serial::write_string(" ");
    kernel::drivers::serial::write_line(info->version);

    kernel::console::terminal::write_string("bootloader = ");
    kernel::console::terminal::write_string(info->name);
    kernel::console::terminal::write_string(" ");
    kernel::console::terminal::write_line(info->version);
}

void write_firmware_info()
{
    const char * firmware = firmware_name(kernel::boot::firmware_type());
    kernel::drivers::serial::write_string("os-lab: firmware = ");
    kernel::drivers::serial::write_line(firmware);
    kernel::console::terminal::write_string("firmware = ");
    kernel::console::terminal::write_line(firmware);
}

void write_loaded_base_revision()
{
    kernel::drivers::serial::write_string("os-lab: loaded base revision = ");
    kernel::drivers::serial::write_decimal(kernel::boot::loaded_base_revision());
    kernel::drivers::serial::write_string("\n");
}

void write_terminal_status(bool terminal_ready)
{
    kernel::drivers::serial::write_line(terminal_ready
                                            ? "os-lab: framebuffer terminal active"
                                            : "os-lab: framebuffer terminal unavailable");
}

void init_mouse_cursor()
{
    const bool mouse_ready = kernel::drivers::mouse::init();
    const bool mouse_cursor_ready = mouse_ready && kernel::display::mouse_cursor::init();
    if (mouse_cursor_ready && mouse_ready)
    {
        const kernel::display::mouse_cursor::Position position = kernel::display::mouse_cursor::position();
        kernel::console::terminal::update_pointer_target(position.x, position.y);
        kernel::display::mouse_cursor::show();
        kernel::drivers::serial::write_line("os-lab: ps/2 mouse cursor active");
    }
    else
    {
        kernel::drivers::serial::write_line("os-lab: ps/2 mouse cursor unavailable");
    }
}

void init_timer_interrupts()
{
    kernel::time::timer::init();
    if (kernel::drivers::keyboard::init_irq())
    {
        kernel::drivers::serial::write_line("os-lab: ps/2 keyboard IRQ active");
    }
    else
    {
        kernel::drivers::serial::write_line("os-lab: ps/2 keyboard IRQ unavailable; polling fallback active");
    }
    kernel::arch::x86_64::enable_interrupts();
    kernel::drivers::serial::write_line("os-lab: hardware interrupts enabled");
}

} // namespace

extern "C" [[noreturn]] void kernel_main()
{
    kernel::drivers::serial::write_line("os-lab: kernel main entered");
    const bool terminal_ready = init_terminal();

    kernel::arch::x86_64::run_exception_smoke();

    run_utility_smoke();
    write_memory_summary(init_memory_and_paging());
    init_kernel_heap();
    init_kernel_slab();
    kernel::debug::run_heap_smoke();
    kernel::debug::run_slab_smoke();
    kernel::debug::run_paging_smoke();
    write_bootloader_info();
    write_firmware_info();
    write_loaded_base_revision();
    write_terminal_status(terminal_ready);
    init_mouse_cursor();
    init_timer_interrupts();
    kernel::debug::run_timer_smoke();

    kernel::shell::run();
}
