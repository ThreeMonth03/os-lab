#include "terminal_display_runtime.hpp"

#include "kernel/boot/limine_support.hpp"
#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/display_palette.hpp"
#include "kernel/display/gui_panel.hpp"

namespace
{

namespace debug_overlay = kernel::display::debug_overlay;
namespace display = kernel::display;
namespace gui_panel = kernel::display::gui_panel;

kernel::console::TerminalDisplayRuntime g_runtime;

uint32_t pack_rgb(const limine_framebuffer & framebuffer, display::RgbColor color)
{
    return (static_cast<uint32_t>(color.red) << framebuffer.red_mask_shift) |
           (static_cast<uint32_t>(color.green) << framebuffer.green_mask_shift) |
           (static_cast<uint32_t>(color.blue) << framebuffer.blue_mask_shift);
}

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

display::Rect framebuffer_bounds(const limine_framebuffer & framebuffer)
{
    return {0, 0, framebuffer.width, framebuffer.height};
}

display::Rect terminal_app_bounds_for(const limine_framebuffer & framebuffer,
                                      display::Rect panel_bounds,
                                      gui_panel::Config panel_config)
{
    const display::Rect full_bounds = framebuffer_bounds(framebuffer);
    if (!panel_config.visible || panel_bounds.empty())
    {
        return full_bounds;
    }

    const uint64_t margin = panel_config.margin;
    const uint64_t left = margin;
    const uint64_t top = panel_bounds.y + panel_bounds.height + margin;
    if (left >= framebuffer.width || top >= framebuffer.height)
    {
        return full_bounds;
    }

    const uint64_t right_margin = max_u64(margin, left);
    if (framebuffer.width <= left + right_margin || framebuffer.height <= top + margin)
    {
        return full_bounds;
    }

    const display::Rect app_bounds{
        left,
        top,
        framebuffer.width - left - right_margin,
        framebuffer.height - top - margin,
    };
    if (app_bounds.width < kernel::console::TerminalApp::kCellWidth ||
        app_bounds.height < kernel::console::TerminalApp::kCellHeight)
    {
        return full_bounds;
    }

    return app_bounds;
}

} // namespace

namespace kernel::console
{

bool TerminalDisplayRuntime::ready() const
{
    return surface_.ready() && terminal_app_surface_.valid() && targets_.active_target().valid();
}

bool TerminalDisplayRuntime::init(
    TerminalApp & terminal_app,
    display::compositor::LayerRepaintCallback terminal_repaint_callback)
{
    const auto * response = boot::framebuffer();
    if (response == nullptr || response->framebuffer_count == 0)
    {
        return false;
    }

    auto * framebuffer = response->framebuffers[0];
    if (framebuffer == nullptr || framebuffer->bpp != 32 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        framebuffer->width < TerminalApp::kCellWidth ||
        framebuffer->height < TerminalApp::kCellHeight)
    {
        return false;
    }

    surface_ = display::Surface(framebuffer->address,
                                framebuffer->width,
                                framebuffer->height,
                                framebuffer->pitch);
    display::compositor::init(framebuffer_bounds(*framebuffer));
    targets_.clear();
    app_surfaces_.clear();
    gui_surfaces_.clear();
    terminal_app_surface_ = {};
    pointer_target_ = {};

    const gui_panel::Config panel_config = gui_panel::default_config();
    const display::Rect panel_bounds =
        gui_panel::bounds_for(framebuffer->width, framebuffer->height, panel_config);
    const display::GuiSurface panel =
        gui_panel::make_surface(framebuffer->width,
                                framebuffer->height,
                                gui_panel::kGuiSurfaceId,
                                panel_config);
    constexpr display::DisplayPalette palette = display::default_display_palette();
    if (gui_surfaces_.register_surface(panel))
    {
        const display::GuiSurface * registered_panel =
            gui_surfaces_.find(gui_panel::kGuiSurfaceId);
        if (registered_panel != nullptr &&
            targets_.register_target(registered_panel->display_target()))
        {
            (void)display::compositor::register_layer({
                display::LayerKind::DesktopPanel,
                registered_panel->display_surface_id,
                framebuffer_bounds(*framebuffer),
                true,
            });

            const display::Color panel_border{pack_rgb(*framebuffer, palette.panel_border)};
            const display::Color panel_background{
                pack_rgb(*framebuffer, palette.desktop_background)};
            const display::Color panel_foreground{
                pack_rgb(*framebuffer, palette.panel_foreground)};
            (void)gui_panel::init(surface_,
                                  *registered_panel,
                                  panel_border,
                                  panel_background,
                                  panel_foreground,
                                  panel_config);
        }
    }

    terminal_app_surface_ =
        display::make_app_surface(display::kTerminalAppSurfaceId,
                                  terminal_app_bounds_for(*framebuffer,
                                                          panel_bounds,
                                                          panel_config),
                                  true,
                                  true);
    if (!app_surfaces_.register_surface(terminal_app_surface_))
    {
        return false;
    }

    const display::AppSurface * registered_app =
        app_surfaces_.find(display::kTerminalAppSurfaceId);
    if (registered_app == nullptr ||
        !targets_.register_target(registered_app->display_target()) ||
        !targets_.set_active(registered_app->display_surface_id) ||
        !targets_.set_focused(registered_app->display_surface_id))
    {
        return false;
    }

    (void)app_surfaces_.set_focused(display::kTerminalAppSurfaceId);
    terminal_app_surface_ = *registered_app;
    (void)display::compositor::register_layer(registered_app->layer());
    (void)display::compositor::register_layer_repaint_callback(display::LayerKind::AppSurface,
                                                               terminal_repaint_callback);

    const display::Rect overlay_bounds =
        debug_overlay::bounds_for(framebuffer->width, framebuffer->height);
    if (!overlay_bounds.empty())
    {
        const bool overlay_registered = targets_.register_target({
            debug_overlay::kSurfaceId,
            display::DisplayTargetKind::DebugOverlay,
            overlay_bounds,
            false,
            false,
        });
        const display::SurfaceDescriptor * overlay_target =
            targets_.find(debug_overlay::kSurfaceId);
        if (overlay_registered && overlay_target != nullptr)
        {
            const display::Color overlay_foreground{
                pack_rgb(*framebuffer, palette.debug_overlay_foreground)};
            const display::Color overlay_background{
                pack_rgb(*framebuffer, palette.debug_overlay_background)};
            (void)debug_overlay::init(surface_,
                                      *overlay_target,
                                      overlay_foreground,
                                      overlay_background);
            (void)display::compositor::register_layer({
                display::LayerKind::DebugOverlay,
                debug_overlay::kSurfaceId,
                overlay_bounds,
                true,
            });
        }
    }

    const display::Color terminal_foreground{
        pack_rgb(*framebuffer, palette.terminal_foreground)};
    const display::Color terminal_background{
        pack_rgb(*framebuffer, palette.terminal_background)};
    if (!terminal_app.reset(surface_,
                            terminal_app_surface_,
                            terminal_foreground,
                            terminal_background))
    {
        return false;
    }

    gui_panel::refresh_now();
    return true;
}

void TerminalDisplayRuntime::update_pointer_target(uint64_t x, uint64_t y)
{
    if (!ready())
    {
        pointer_target_ = {};
        return;
    }

    pointer_target_ = display::hit_test(targets_, app_surfaces_, gui_surfaces_, x, y);
}

TerminalDisplayRuntime & terminal_display_runtime()
{
    return g_runtime;
}

} // namespace kernel::console
