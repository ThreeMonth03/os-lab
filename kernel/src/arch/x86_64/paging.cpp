#include "kernel/arch/x86_64/paging.hpp"

namespace kernel::arch::x86_64::paging
{

uint64_t align_down_to_page(uint64_t value) { return value & ~kPageOffsetMask; }

uint64_t align_up_to_page(uint64_t value)
{
    if (is_page_aligned(value))
    {
        return value;
    }

    if (value > UINT64_MAX - kPageOffsetMask)
    {
        return 0;
    }

    return (value + kPageOffsetMask) & ~kPageOffsetMask;
}

bool is_page_aligned(uint64_t value) { return (value & kPageOffsetMask) == 0; }

PageIndices page_indices(uint64_t virtual_address)
{
    return {
        static_cast<uint16_t>((virtual_address >> 39) & kPageTableIndexMask),
        static_cast<uint16_t>((virtual_address >> 30) & kPageTableIndexMask),
        static_cast<uint16_t>((virtual_address >> 21) & kPageTableIndexMask),
        static_cast<uint16_t>((virtual_address >> 12) & kPageTableIndexMask),
        static_cast<uint16_t>(virtual_address & kPageOffsetMask),
    };
}

PageTableManager::PageTableManager(PageTable & root,
                                   AllocatePageTable allocate_table,
                                   PageTableFromPhysical table_from_physical,
                                   void * context)
    : root_(&root)
    , allocate_table_(allocate_table)
    , table_from_physical_(table_from_physical)
    , context_(context)
{
}

MapResult PageTableManager::map_page(uint64_t virtual_address,
                                     uint64_t physical_address,
                                     PageFlags flags)
{
    if (!is_page_aligned(virtual_address) || !is_page_aligned(physical_address))
    {
        return MapResult::InvalidAlignment;
    }

    if (root_ == nullptr || allocate_table_ == nullptr || table_from_physical_ == nullptr)
    {
        return MapResult::AllocationFailed;
    }

    const PageIndices indices = page_indices(virtual_address);
    PageTable * table = root_;
    PageTable * next = nullptr;

    if (!ensure_next_table(table->entries[indices.pml4], flags, next))
    {
        return MapResult::AllocationFailed;
    }
    table = next;

    if (!ensure_next_table(table->entries[indices.pdpt], flags, next))
    {
        return MapResult::AllocationFailed;
    }
    table = next;

    if (!ensure_next_table(table->entries[indices.pd], flags, next))
    {
        return MapResult::AllocationFailed;
    }
    table = next;

    PageTableEntry & leaf = table->entries[indices.pt];
    if (leaf.present())
    {
        return MapResult::AlreadyMapped;
    }

    leaf.set(physical_address, flags);
    return MapResult::Mapped;
}

bool PageTableManager::translate(uint64_t virtual_address, Translation & translation) const
{
    if (root_ == nullptr || table_from_physical_ == nullptr)
    {
        translation = {};
        return false;
    }

    const PageIndices indices = page_indices(virtual_address);
    const PageTable * table = root_;

    const PageTableEntry * entry = &table->entries[indices.pml4];
    if (!entry->present() || entry->flags().contains(PageFlag::HugePage))
    {
        translation = {};
        return false;
    }

    table = table_for(*entry);
    if (table == nullptr)
    {
        translation = {};
        return false;
    }

    entry = &table->entries[indices.pdpt];
    if (!entry->present() || entry->flags().contains(PageFlag::HugePage))
    {
        translation = {};
        return false;
    }

    table = table_for(*entry);
    if (table == nullptr)
    {
        translation = {};
        return false;
    }

    entry = &table->entries[indices.pd];
    if (!entry->present() || entry->flags().contains(PageFlag::HugePage))
    {
        translation = {};
        return false;
    }

    table = table_for(*entry);
    if (table == nullptr)
    {
        translation = {};
        return false;
    }

    entry = &table->entries[indices.pt];
    if (!entry->present())
    {
        translation = {};
        return false;
    }

    translation = {entry->address() + indices.offset, entry->flags()};
    return true;
}

PageTable * PageTableManager::table_for(const PageTableEntry & entry) const
{
    return table_from_physical_(entry.address(), context_);
}

bool PageTableManager::ensure_next_table(PageTableEntry & entry, PageFlags leaf_flags, PageTable *& next)
{
    if (entry.present())
    {
        if (entry.flags().contains(PageFlag::HugePage))
        {
            next = nullptr;
            return false;
        }

        next = table_for(entry);
        return next != nullptr;
    }

    PageTable * table = nullptr;
    uint64_t physical_address = 0;
    if (!allocate_table_(table, physical_address, context_) || table == nullptr ||
        !is_page_aligned(physical_address))
    {
        next = nullptr;
        return false;
    }

    PageFlags table_flags = PageFlag::Present | PageFlag::Writable;
    if (leaf_flags.contains(PageFlag::User))
    {
        table_flags = table_flags | PageFlag::User;
    }

    entry.set(physical_address, table_flags);
    next = table;
    return true;
}

} // namespace kernel::arch::x86_64::paging
