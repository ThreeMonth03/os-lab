#pragma once

#include <stdint.h>

#include "kernel/arch/x86_64/paging.hpp"

namespace kernel::arch::x86_64::paging
{

class ActivePageTable
{
public:
    constexpr ActivePageTable() = default;
    ActivePageTable(PageTable & root, uint64_t root_physical);

    [[nodiscard]] static bool current(ActivePageTable & active);

    [[nodiscard]] bool valid() const { return root_ != nullptr; }
    [[nodiscard]] uint64_t root_physical() const { return root_physical_; }

    [[nodiscard]] MapResult map_page(uint64_t virtual_address,
                                     uint64_t physical_address,
                                     PageFlags flags);
    [[nodiscard]] UnmapResult unmap_page(uint64_t virtual_address);
    [[nodiscard]] bool translate(uint64_t virtual_address, Translation & translation) const;

private:
    PageTable * root_ = nullptr;
    uint64_t root_physical_ = 0;
};

[[nodiscard]] uint64_t read_cr3_physical_address();
void flush_tlb_page(uint64_t virtual_address);

} // namespace kernel::arch::x86_64::paging
