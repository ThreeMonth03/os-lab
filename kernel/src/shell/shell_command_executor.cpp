#include "shell_command_executor.hpp"

#include <stdint.h>

#include "kernel/core/halt.hpp"
#include "kernel/display/mouse_cursor.hpp"
#include "kernel/input/input.hpp"
#include "kernel/input/input_router.hpp"
#include "kernel/memory/heap.hpp"
#include "kernel/memory/memory.hpp"
#include "kernel/memory/slab.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/shell/shell_command.hpp"
#include "kernel/console/terminal.hpp"

namespace
{

namespace serial = kernel::drivers::serial;
namespace mouse_cursor = kernel::display::mouse_cursor;
namespace terminal = kernel::console::terminal;

void write_help()
{
    terminal::write_line("commands:");
    terminal::write_line("  help  - show this list");
    terminal::write_line("  clear - clear the screen");
    terminal::write_line("  about - show kernel info");
    terminal::write_line("  input - show input stats");
    terminal::write_line("  mem   - show memory stats");
    terminal::write_line("  heap  - show heap stats");
    terminal::write_line("  slab  - show slab cache stats");
    terminal::write_line("  halt  - stop the cpu");
}

void write_about()
{
    terminal::write_line("os-lab early shell");
    terminal::write_line("freestanding c++23 kernel");
    terminal::write_line("no filesystem yet");
}

void write_stat(kernel::StringView name, uint64_t value)
{
    terminal::write_string("  ");
    terminal::write_string(name);
    terminal::write_string(": ");
    terminal::write_decimal(value);
    terminal::write_char('\n');
}

void write_hex_stat(kernel::StringView name, uint64_t value)
{
    terminal::write_string("  ");
    terminal::write_string(name);
    terminal::write_string(": ");
    terminal::write_hex(value);
    terminal::write_char('\n');
}

void write_bool_stat(kernel::StringView name, bool value)
{
    terminal::write_string("  ");
    terminal::write_string(name);
    terminal::write_string(": ");
    terminal::write_line(value ? "yes" : "no");
}

kernel::StringView input_device_mode_name(kernel::input::DeviceMode mode)
{
    switch (mode)
    {
    case kernel::input::DeviceMode::PollingFallback:
        return "polling fallback";
    case kernel::input::DeviceMode::Irq:
        return "irq";
    }
    return "unknown";
}

kernel::StringView input_focus_name(kernel::input::InputFocus focus)
{
    switch (focus)
    {
    case kernel::input::InputFocus::None:
        return "none";
    case kernel::input::InputFocus::Shell:
        return "shell";
    case kernel::input::InputFocus::TerminalApp:
        return "terminal app";
    case kernel::input::InputFocus::GuiSurface:
        return "gui surface";
    }
    return "unknown";
}

kernel::StringView pointer_target_name(kernel::display::DisplayTargetKind kind)
{
    switch (kind)
    {
    case kernel::display::DisplayTargetKind::None:
        return "none";
    case kernel::display::DisplayTargetKind::AppSurface:
        return "app surface";
    case kernel::display::DisplayTargetKind::DebugOverlay:
        return "debug overlay";
    case kernel::display::DisplayTargetKind::GuiSurface:
        return "gui surface";
    }
    return "unknown";
}

kernel::StringView heap_validation_error_name(kernel::memory::HeapValidationError error)
{
    switch (error)
    {
    case kernel::memory::HeapValidationError::None:
        return "none";
    case kernel::memory::HeapValidationError::RegionListFull:
        return "region list full";
    case kernel::memory::HeapValidationError::RegionMisaligned:
        return "region misaligned";
    case kernel::memory::HeapValidationError::RegionTooSmall:
        return "region too small";
    case kernel::memory::HeapValidationError::RegionOverlap:
        return "region overlap";
    case kernel::memory::HeapValidationError::RegionStatsMismatch:
        return "region stats mismatch";
    case kernel::memory::HeapValidationError::FreeListPreviousMismatch:
        return "free list previous mismatch";
    case kernel::memory::HeapValidationError::FreeListOrder:
        return "free list order";
    case kernel::memory::HeapValidationError::FreeBlockMisaligned:
        return "free block misaligned";
    case kernel::memory::HeapValidationError::FreeBlockTooSmall:
        return "free block too small";
    case kernel::memory::HeapValidationError::FreeBlockSizeMisaligned:
        return "free block size misaligned";
    case kernel::memory::HeapValidationError::FreeBlockOutOfRegion:
        return "free block out of region";
    case kernel::memory::HeapValidationError::FreeStatsMismatch:
        return "free stats mismatch";
    case kernel::memory::HeapValidationError::AllocatedBytesExceedRegion:
        return "allocated bytes exceed region";
    }
    return "unknown";
}

kernel::StringView slab_validation_error_name(kernel::memory::SlabValidationError error)
{
    switch (error)
    {
    case kernel::memory::SlabValidationError::None:
        return "none";
    case kernel::memory::SlabValidationError::InvalidConfig:
        return "invalid config";
    case kernel::memory::SlabValidationError::SlabHeaderCorrupt:
        return "slab header corrupt";
    case kernel::memory::SlabValidationError::SlabOutOfRange:
        return "slab out of range";
    case kernel::memory::SlabValidationError::SlabOverlap:
        return "slab overlap";
    case kernel::memory::SlabValidationError::FreeListOutOfSlab:
        return "free list out of slab";
    case kernel::memory::SlabValidationError::FreeListDuplicate:
        return "free list duplicate";
    case kernel::memory::SlabValidationError::FreeStatsMismatch:
        return "free stats mismatch";
    case kernel::memory::SlabValidationError::StatsMismatch:
        return "stats mismatch";
    }
    return "unknown";
}

kernel::StringView slab_registry_validation_error_name(kernel::memory::SlabRegistryValidationError error)
{
    switch (error)
    {
    case kernel::memory::SlabRegistryValidationError::None:
        return "none";
    case kernel::memory::SlabRegistryValidationError::MissingStorage:
        return "missing storage";
    case kernel::memory::SlabRegistryValidationError::DuplicateName:
        return "duplicate name";
    case kernel::memory::SlabRegistryValidationError::CacheInvalid:
        return "cache invalid";
    case kernel::memory::SlabRegistryValidationError::StatsMismatch:
        return "stats mismatch";
    }
    return "unknown";
}

void write_input_stats()
{
    const kernel::input::Stats stats = kernel::input::stats();
    const kernel::display::HitTestResult pointer_target = terminal::pointer_target();
    const mouse_cursor::Position pointer_position = mouse_cursor::position();

    terminal::write_line("input stats:");
    terminal::write_string("  focus: ");
    terminal::write_line(input_focus_name(kernel::input::focus()));
    terminal::write_string("  pointer target kind: ");
    terminal::write_line(pointer_target_name(pointer_target.target_kind));
    write_stat("pointer x", pointer_position.x);
    write_stat("pointer y", pointer_position.y);
    write_stat("pointer display surface id", pointer_target.surface_id);
    if (pointer_target.target_kind == kernel::display::DisplayTargetKind::AppSurface)
    {
        write_stat("pointer app surface id", pointer_target.app_surface_id);
    }
    if (pointer_target.target_kind == kernel::display::DisplayTargetKind::GuiSurface)
    {
        write_stat("pointer gui surface id", pointer_target.gui_surface_id);
    }
    terminal::write_string("  keyboard mode: ");
    terminal::write_line(input_device_mode_name(stats.keyboard_mode));
    terminal::write_string("  mouse mode: ");
    terminal::write_line(input_device_mode_name(stats.mouse_mode));
    write_stat("key events", stats.key_events);
    write_stat("keyboard IRQ events", stats.keyboard_irq_events);
    write_stat("keyboard polling fallback events", stats.keyboard_polling_fallback_events);
    write_stat("mouse move events", stats.mouse_move_events);
    write_stat("mouse IRQ events", stats.mouse_irq_events);
    write_stat("mouse polling fallback events", stats.mouse_polling_fallback_events);
    write_stat("dropped events", stats.dropped_events);
    write_stat("queued events", stats.queued_events);
    write_stat("queue available", stats.queue_available);
    write_stat("queue capacity", stats.queue_capacity);
}

void write_memory_stats()
{
    const kernel::memory::Stats stats = kernel::memory::stats();
    if (!stats.initialized)
    {
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

void write_heap_stats()
{
    const kernel::memory::heap::Stats stats = kernel::memory::heap::stats();
    const kernel::memory::HeapValidationResult validation = kernel::memory::heap::validate();
    if (!stats.initialized)
    {
        terminal::write_line("heap stats unavailable");
        return;
    }

    terminal::write_line("heap stats:");
    write_bool_stat("initialized", stats.initialized);
    write_bool_stat("valid", validation.valid);
    terminal::write_string("  validation error: ");
    terminal::write_line(heap_validation_error_name(validation.error));
    write_hex_stat("virtual base", stats.virtual_base);
    write_stat("committed bytes", stats.committed_bytes);
    write_stat("max bytes", stats.reserved_bytes);
    write_stat("committed pages", stats.committed_pages);
    write_stat("regions", stats.allocator.region_count);
    write_stat("allocated bytes", stats.allocator.allocated_bytes);
    write_stat("allocation count", stats.allocator.allocation_count);
    write_stat("free bytes", stats.allocator.free_bytes);
    write_stat("free block count", stats.allocator.free_block_count);
    write_stat("largest free block", stats.allocator.largest_free_block);
    write_stat("failed allocations", stats.failed_allocations);
}

void write_slab_stats()
{
    const kernel::memory::slab::Stats stats = kernel::memory::slab::stats();
    const kernel::memory::SlabRegistryValidationResult validation = kernel::memory::slab::validate_all();
    if (!stats.initialized)
    {
        terminal::write_line("slab stats unavailable");
        return;
    }

    terminal::write_line("slab stats:");
    write_bool_stat("initialized", stats.initialized);
    write_bool_stat("valid", validation.valid);
    terminal::write_string("  validation error: ");
    terminal::write_line(slab_registry_validation_error_name(validation.error));
    if (validation.error == kernel::memory::SlabRegistryValidationError::CacheInvalid)
    {
        write_stat("invalid cache id", validation.cache_id);
        terminal::write_string("  cache error: ");
        terminal::write_line(slab_validation_error_name(validation.cache_error));
    }
    write_stat("cache capacity", stats.registry.capacity);
    write_stat("registered caches", stats.registry.registered_caches);
    write_stat("failed registrations", stats.registry.failed_registrations);
    write_stat("failed backing allocations", stats.failed_backing_allocations);

    for (kernel::memory::SlabCacheId id = 0; id < stats.registry.capacity; ++id)
    {
        const kernel::memory::SlabRegistryCacheStats cache = kernel::memory::slab::cache_stats(id);
        if (!cache.registered)
        {
            continue;
        }

        terminal::write_string("cache ");
        terminal::write_decimal(id);
        terminal::write_string(" ");
        terminal::write_line(cache.name);
        write_stat("object size", cache.cache.object_size);
        write_stat("alignment", cache.cache.alignment);
        write_stat("slab count", cache.cache.slab_count);
        write_stat("committed bytes", cache.cache.committed_bytes);
        write_stat("object capacity", cache.cache.object_capacity);
        write_stat("allocated objects", cache.cache.allocated_objects);
        write_stat("free objects", cache.cache.free_objects);
        write_stat("failed allocations", cache.cache.failed_allocations);
        write_stat("failed backing allocations", cache.failed_backing_allocations);
    }
}

} // namespace

namespace kernel::shell
{

void execute_command(StringView command)
{
    const ShellCommand parsed = parse_shell_command(command);

    if (parsed.kind == ShellCommandKind::Empty)
    {
        return;
    }

    terminal::ScopedUpdate terminal_update;
    switch (parsed.kind)
    {
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
    case ShellCommandKind::Heap:
        write_heap_stats();
        break;
    case ShellCommandKind::Slab:
        write_slab_stats();
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
