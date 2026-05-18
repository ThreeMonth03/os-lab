#include "kernel/debug/terminal_app_smoke.hpp"

#include <stdint.h>

#include "display_runtime_app.hpp"
#include "kernel/core/halt.hpp"
#include "kernel/display/app_surface.hpp"
#include "kernel/display/display.hpp"
#include "kernel/drivers/serial.hpp"

#ifndef OS_LAB_TERMINAL_APP_LIFECYCLE_SMOKE
#define OS_LAB_TERMINAL_APP_LIFECYCLE_SMOKE 0
#endif

namespace
{

#if OS_LAB_TERMINAL_APP_LIFECYCLE_SMOKE

constexpr uint64_t kResizeInset = 32;

void write_line(const char * message)
{
    kernel::drivers::serial::write_line(message);
}

[[noreturn]] void fail(const char * reason)
{
    kernel::drivers::serial::write_string("os-lab: terminal app lifecycle smoke failed: ");
    kernel::drivers::serial::write_line(reason);
    kernel::halt_forever();
}

kernel::display::Rect smaller_bounds(kernel::display::Rect bounds)
{
    if (bounds.width <= (kResizeInset * 3) || bounds.height <= (kResizeInset * 3))
    {
        return {};
    }

    return {
        bounds.x + kResizeInset,
        bounds.y + kResizeInset,
        bounds.width - (kResizeInset * 2),
        bounds.height - (kResizeInset * 2),
    };
}

void require_surface_open(kernel::display::AppSurface surface, const char * context)
{
    if (!surface.valid() || !surface.open())
    {
        fail(context);
    }
}

void run_enabled_smoke()
{
    using namespace kernel::display;

    write_line("os-lab: terminal app lifecycle smoke started");

    const runtime::AppSurfaceHostConfig initial_config = runtime::primary_app_config();
    require_surface_open(initial_config.surface, "initial app surface invalid");

    const Rect initial_bounds = initial_config.surface.bounds;
    const Rect resized_bounds = smaller_bounds(initial_bounds);
    if (resized_bounds.empty())
    {
        fail("smaller bounds unavailable");
    }

    if (!runtime::resize_app_surface(kTerminalAppSurfaceId, resized_bounds))
    {
        fail("resize smaller failed");
    }
    require_surface_open(runtime::primary_app_config().surface, "surface invalid after resize smaller");

    if (!runtime::resize_app_surface(kTerminalAppSurfaceId, initial_bounds))
    {
        fail("resize default failed");
    }
    require_surface_open(runtime::primary_app_config().surface, "surface invalid after resize default");

    if (!runtime::set_app_surface_visible(kTerminalAppSurfaceId, false))
    {
        fail("hide failed");
    }
    if (!runtime::primary_app_config().surface.hidden())
    {
        fail("surface not hidden");
    }
    if (runtime::focus_app_surface(kTerminalAppSurfaceId))
    {
        fail("focus hidden unexpectedly succeeded");
    }

    if (!runtime::set_app_surface_visible(kTerminalAppSurfaceId, true))
    {
        fail("show failed");
    }
    require_surface_open(runtime::primary_app_config().surface, "surface invalid after show");

    if (!runtime::clear_app_surface_focus(kTerminalAppSurfaceId))
    {
        fail("clear focus failed");
    }
    if (runtime::primary_app_config().surface.focused)
    {
        fail("surface still focused after clear focus");
    }

    if (!runtime::focus_app_surface(kTerminalAppSurfaceId))
    {
        fail("focus failed");
    }
    if (!runtime::primary_app_config().surface.focused)
    {
        fail("surface not focused");
    }

    if (!runtime::close_app_surface(kTerminalAppSurfaceId))
    {
        fail("close failed");
    }
    if (!runtime::primary_app_config().surface.closed())
    {
        fail("surface not closed");
    }

    write_line("os-lab: terminal app lifecycle smoke passed");
    kernel::halt_forever();
}

#endif

} // namespace

namespace kernel::debug
{

void run_terminal_app_lifecycle_smoke()
{
#if OS_LAB_TERMINAL_APP_LIFECYCLE_SMOKE
    run_enabled_smoke();
#endif
}

} // namespace kernel::debug
