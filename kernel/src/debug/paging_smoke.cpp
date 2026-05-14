#include "kernel/debug/paging_smoke.hpp"

#include "kernel/arch/x86_64/active_paging.hpp"
#include "kernel/base/string_view.hpp"
#include "kernel/console/terminal.hpp"
#include "kernel/core/halt.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/memory/memory.hpp"

#ifndef OS_LAB_PAGING_SMOKE
#define OS_LAB_PAGING_SMOKE 0
#endif

namespace
{

#if OS_LAB_PAGING_SMOKE

namespace paging = kernel::arch::x86_64::paging;

// Debug-only scratch page. Limine currently places HHDM near 0xffff8000...
// and the kernel at 0xffffffff80000000, so this PML4 slot is kept as a
// temporary smoke-test window rather than a committed heap range.
constexpr uint64_t kScratchVirtualAddress = 0xffff900000000000;
constexpr uint64_t kSentinelValue = 0x6f735f6c61625f76;

void write_both(kernel::StringView value)
{
    kernel::drivers::serial::write_string(value);
    if (kernel::console::terminal::ready())
    {
        kernel::console::terminal::write_string(value);
    }
}

void write_both_line(kernel::StringView value)
{
    kernel::drivers::serial::write_line(value);
    if (kernel::console::terminal::ready())
    {
        kernel::console::terminal::write_line(value);
    }
}

[[noreturn]] void fail(kernel::StringView reason)
{
    write_both("os-lab: paging smoke failed: ");
    write_both_line(reason);
    kernel::halt_forever();
}

#endif

} // namespace

namespace kernel::debug
{

void run_paging_smoke()
{
#if OS_LAB_PAGING_SMOKE
    write_both_line("os-lab: paging smoke mapping scratch page");

    paging::ActivePageTable active;
    if (!paging::ActivePageTable::current(active))
    {
        fail("active page table unavailable");
    }

    kernel::memory::PhysicalFrame frame;
    if (!kernel::memory::allocate_frame(frame))
    {
        fail("frame allocation failed");
    }

    const paging::MapResult map_result =
        active.map_page(kScratchVirtualAddress, frame.address(), paging::PageFlag::Writable | paging::PageFlag::NoExecute);
    if (map_result != paging::MapResult::Mapped)
    {
        fail("map failed");
    }

    auto * scratch = reinterpret_cast<volatile uint64_t *>(kScratchVirtualAddress);
    *scratch = kSentinelValue;
    if (*scratch != kSentinelValue)
    {
        fail("mapped page readback failed");
    }

    paging::Translation translation;
    if (!active.translate(kScratchVirtualAddress, translation) || translation.physical_address != frame.address())
    {
        fail("translate failed");
    }

    if (active.unmap_page(kScratchVirtualAddress) != paging::UnmapResult::Unmapped)
    {
        fail("unmap failed");
    }
    if (active.translate(kScratchVirtualAddress, translation))
    {
        fail("translate after unmap succeeded");
    }

    write_both_line("os-lab: paging smoke passed");
    kernel::halt_forever();
#endif
}

} // namespace kernel::debug
