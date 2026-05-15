#include "kernel/debug/slab_smoke.hpp"

#include <stddef.h>
#include <stdint.h>

#include "kernel/base/string_view.hpp"
#include "kernel/console/terminal.hpp"
#include "kernel/core/halt.hpp"
#include "kernel/drivers/serial.hpp"
#include "kernel/memory/slab.hpp"

#ifndef OS_LAB_SLAB_SMOKE
#define OS_LAB_SLAB_SMOKE 0
#endif

namespace
{

#if OS_LAB_SLAB_SMOKE

constexpr uint64_t kFirstSentinel = 0x534c4142534d4f31;
constexpr uint64_t kSecondSentinel = 0x534c4142534d4f32;
constexpr uint64_t kThirdSentinel = 0x534c4142534d4f33;

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
    write_both("os-lab: slab smoke failed: ");
    write_both_line(reason);
    kernel::halt_forever();
}

void require_valid_slab(kernel::StringView context)
{
    if (!kernel::memory::slab::validate_all().valid)
    {
        fail(context);
    }
}

[[nodiscard]] bool aligned(void * memory, size_t alignment)
{
    return (reinterpret_cast<uintptr_t>(memory) & (alignment - 1)) == 0;
}

#endif

} // namespace

namespace kernel::debug
{

void run_slab_smoke()
{
#if OS_LAB_SLAB_SMOKE
    write_both_line("os-lab: slab smoke allocating objects");

    if (!kernel::memory::slab::init())
    {
        fail("slab init failed");
    }
    require_valid_slab("slab validate failed after init");

    const kernel::memory::SlabCacheId cache_id =
        kernel::memory::slab::create_cache("smoke64",
                                           kernel::memory::slab::kDefaultObjectSize,
                                           kernel::memory::slab::kDefaultAlignment);
    if (cache_id == kernel::memory::kInvalidSlabCacheId)
    {
        fail("smoke cache registration failed");
    }

    auto * first = static_cast<volatile uint64_t *>(kernel::memory::slab::allocate(cache_id));
    auto * second = static_cast<volatile uint64_t *>(kernel::memory::slab::allocate(cache_id));
    auto * third = static_cast<volatile uint64_t *>(kernel::memory::slab::allocate(cache_id));
    if (first == nullptr || second == nullptr || third == nullptr)
    {
        fail("allocation returned null");
    }
    if (!aligned(const_cast<uint64_t *>(first), kernel::memory::slab::kDefaultAlignment) ||
        !aligned(const_cast<uint64_t *>(second), kernel::memory::slab::kDefaultAlignment) ||
        !aligned(const_cast<uint64_t *>(third), kernel::memory::slab::kDefaultAlignment))
    {
        fail("allocation alignment failed");
    }

    *first = kFirstSentinel;
    *second = kSecondSentinel;
    *third = kThirdSentinel;
    if (*first != kFirstSentinel || *second != kSecondSentinel || *third != kThirdSentinel)
    {
        fail("sentinel readback failed");
    }

    if (!kernel::memory::slab::free(cache_id, const_cast<uint64_t *>(first)))
    {
        fail("free failed");
    }
    require_valid_slab("slab validate failed after free");

    void * reused = kernel::memory::slab::allocate(cache_id);
    if (reused != const_cast<uint64_t *>(first))
    {
        fail("free object reuse failed");
    }
    require_valid_slab("slab validate failed after reuse");

    uint64_t outside_slab = 0;
    if (kernel::memory::slab::free(&outside_slab))
    {
        fail("invalid free unexpectedly succeeded");
    }
    require_valid_slab("slab validate failed after invalid free");

    const kernel::memory::slab::Stats stats = kernel::memory::slab::stats();
    const kernel::memory::SlabRegistryCacheStats cache_stats = kernel::memory::slab::cache_stats(cache_id);
    if (!stats.initialized || stats.registry.registered_caches < 2 || !cache_stats.registered ||
        cache_stats.cache.slab_count == 0 || cache_stats.cache.allocated_objects != 3 ||
        !kernel::memory::slab::validate_all().valid)
    {
        fail("unexpected stats");
    }

    write_both_line("os-lab: slab smoke passed");
    kernel::halt_forever();
#endif
}

} // namespace kernel::debug
