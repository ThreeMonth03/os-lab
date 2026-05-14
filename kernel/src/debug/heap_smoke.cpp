#include "kernel/debug/heap_smoke.hpp"

#include <stddef.h>
#include <stdint.h>

#include "kernel/base/string_view.hpp"
#include "kernel/console/terminal.hpp"
#include "kernel/core/halt.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/memory/heap.hpp"

#ifndef OS_LAB_HEAP_SMOKE
#define OS_LAB_HEAP_SMOKE 0
#endif

namespace
{

#if OS_LAB_HEAP_SMOKE

constexpr uint64_t kFirstSentinel = 0x48454150534d4f31;
constexpr uint64_t kSecondSentinel = 0x48454150534d4f32;
constexpr uint64_t kLargeSentinel = 0x48454150534d4f33;

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
    write_both("os-lab: heap smoke failed: ");
    write_both_line(reason);
    kernel::halt_forever();
}

[[nodiscard]] bool aligned(void * memory, size_t alignment)
{
    return (reinterpret_cast<uintptr_t>(memory) & (alignment - 1)) == 0;
}

void require_valid_heap(kernel::StringView context)
{
    if (!kernel::memory::heap::validate().valid)
    {
        fail(context);
    }
}

#endif

} // namespace

namespace kernel::debug
{

void run_heap_smoke()
{
#if OS_LAB_HEAP_SMOKE
    write_both_line("os-lab: heap smoke allocating blocks");

    if (!kernel::memory::heap::init())
    {
        fail("heap init failed");
    }
    require_valid_heap("heap validate failed after init");
    const kernel::memory::heap::Stats baseline_stats = kernel::memory::heap::stats();

    auto * first = static_cast<volatile uint64_t *>(kernel::memory::heap::allocate(sizeof(uint64_t), 16));
    auto * second = static_cast<volatile uint64_t *>(kernel::memory::heap::allocate(sizeof(uint64_t), 64));
    auto * large = static_cast<volatile uint64_t *>(kernel::memory::heap::allocate(20 * 1024, 4096));
    if (first == nullptr || second == nullptr || large == nullptr)
    {
        fail("allocation returned null");
    }
    if (!aligned(const_cast<uint64_t *>(first), 16) || !aligned(const_cast<uint64_t *>(second), 64) ||
        !aligned(const_cast<uint64_t *>(large), 4096))
    {
        fail("allocation alignment failed");
    }

    *first = kFirstSentinel;
    *second = kSecondSentinel;
    large[0] = kLargeSentinel;
    large[(20 * 1024 / sizeof(uint64_t)) - 1] = kLargeSentinel;
    if (*first != kFirstSentinel || *second != kSecondSentinel || large[0] != kLargeSentinel ||
        large[(20 * 1024 / sizeof(uint64_t)) - 1] != kLargeSentinel)
    {
        fail("sentinel readback failed");
    }

    if (!kernel::memory::heap::free(const_cast<uint64_t *>(first)))
    {
        fail("free failed");
    }
    require_valid_heap("heap validate failed after free");

    void * reused = kernel::memory::heap::allocate(sizeof(uint64_t), 16);
    if (reused != const_cast<uint64_t *>(first))
    {
        fail("free block reuse failed");
    }
    require_valid_heap("heap validate failed after reuse");

    uint64_t outside_heap = 0;
    if (kernel::memory::heap::free(&outside_heap))
    {
        fail("invalid free unexpectedly succeeded");
    }
    require_valid_heap("heap validate failed after invalid free");

    const kernel::memory::heap::Stats stats = kernel::memory::heap::stats();
    if (!stats.initialized || stats.committed_pages <= baseline_stats.committed_pages ||
        stats.allocator.allocation_count != baseline_stats.allocator.allocation_count + 3)
    {
        fail("unexpected stats");
    }

    write_both_line("os-lab: heap smoke passed");
    kernel::halt_forever();
#endif
}

} // namespace kernel::debug
