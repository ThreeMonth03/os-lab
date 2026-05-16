#include "kernel/arch/x86_64/paging.hpp"

#include "kernel/boot/limine_support.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/memory/memory.hpp"

namespace
{

namespace paging = kernel::arch::x86_64::paging;
namespace serial = kernel::drivers::serial;

constexpr uint64_t kScratchVirtualAddress = 0xffffffff90000000;

struct PagingFoundationState
{
    paging::PageTable * root = nullptr;
    uint64_t root_physical = 0;
    bool initialized = false;
};

PagingFoundationState g_state;

void clear_table(paging::PageTable & table)
{
    for (paging::PageTableEntry & entry : table.entries)
    {
        entry.clear();
    }
}

paging::PageTable * table_from_physical(uint64_t physical_address, void *)
{
    if (!kernel::boot::hhdm_available())
    {
        return nullptr;
    }

    return reinterpret_cast<paging::PageTable *>(physical_address + kernel::boot::hhdm_offset());
}

bool allocate_table(paging::PageTable *& table, uint64_t & physical_address, void *)
{
    kernel::memory::PhysicalFrame frame;
    if (!kernel::memory::allocate_frame(frame))
    {
        table = nullptr;
        physical_address = 0;
        return false;
    }

    physical_address = frame.address();
    table = table_from_physical(physical_address, nullptr);
    if (table == nullptr)
    {
        return false;
    }

    clear_table(*table);
    return true;
}

void log_unavailable(const char * reason)
{
    serial::write_string("os-lab: paging foundation unavailable: ");
    serial::write_line(reason);
}

} // namespace

namespace kernel::arch::x86_64::paging
{

bool init_foundation()
{
    if (g_state.initialized)
    {
        return true;
    }

    if (!kernel::boot::hhdm_available())
    {
        log_unavailable("missing HHDM");
        return false;
    }

    PageTable * root = nullptr;
    uint64_t root_physical = 0;
    if (!allocate_table(root, root_physical, nullptr))
    {
        log_unavailable("page table frame allocation failed");
        return false;
    }

    PageTableManager manager(*root, allocate_table, table_from_physical, nullptr);
    const MapResult result =
        manager.map_page(kScratchVirtualAddress, root_physical, PageFlag::Writable | PageFlag::NoExecute);
    if (result != MapResult::Mapped)
    {
        log_unavailable("scratch mapping failed");
        return false;
    }

    Translation translation;
    if (!manager.translate(kScratchVirtualAddress, translation) ||
        translation.physical_address != root_physical)
    {
        log_unavailable("scratch translation failed");
        return false;
    }

    g_state.root = root;
    g_state.root_physical = root_physical;
    g_state.initialized = true;
    serial::write_line("os-lab: x86_64 paging foundation ready");
    return true;
}

} // namespace kernel::arch::x86_64::paging
