#include "kernel/arch/x86_64/active_paging.hpp"

#include "kernel/boot/limine_support.hpp"
#include "kernel/memory/memory.hpp"

namespace
{

namespace paging = kernel::arch::x86_64::paging;

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

} // namespace

namespace kernel::arch::x86_64::paging
{

ActivePageTable::ActivePageTable(PageTable & root, uint64_t root_physical)
    : root_(&root)
    , root_physical_(root_physical)
{
}

bool ActivePageTable::current(ActivePageTable & active)
{
    active = {};
    if (!kernel::boot::hhdm_available())
    {
        return false;
    }

    const uint64_t root_physical = read_cr3_physical_address();
    PageTable * root = table_from_physical(root_physical, nullptr);
    if (root == nullptr)
    {
        return false;
    }

    active = ActivePageTable(*root, root_physical);
    return true;
}

MapResult ActivePageTable::map_page(uint64_t virtual_address,
                                    uint64_t physical_address,
                                    PageFlags flags)
{
    if (root_ == nullptr)
    {
        return MapResult::AllocationFailed;
    }

    PageTableManager manager(*root_, allocate_table, table_from_physical, nullptr);
    const MapResult result = manager.map_page(virtual_address, physical_address, flags);
    if (result == MapResult::Mapped)
    {
        flush_tlb_page(virtual_address);
    }
    return result;
}

UnmapResult ActivePageTable::unmap_page(uint64_t virtual_address)
{
    if (root_ == nullptr)
    {
        return UnmapResult::NotMapped;
    }

    PageTableManager manager(*root_, allocate_table, table_from_physical, nullptr);
    const UnmapResult result = manager.unmap_page(virtual_address);
    if (result == UnmapResult::Unmapped)
    {
        flush_tlb_page(virtual_address);
    }
    return result;
}

bool ActivePageTable::translate(uint64_t virtual_address, Translation & translation) const
{
    if (root_ == nullptr)
    {
        translation = {};
        return false;
    }

    PageTableManager manager(*root_, allocate_table, table_from_physical, nullptr);
    return manager.translate(virtual_address, translation);
}

uint64_t read_cr3_physical_address()
{
    uint64_t value = 0;
    asm volatile("mov %%cr3, %0" : "=r"(value));
    return value & kPageAddressMask;
}

void flush_tlb_page(uint64_t virtual_address)
{
    asm volatile("invlpg (%0)" : : "r"(virtual_address) : "memory");
}

} // namespace kernel::arch::x86_64::paging
