#include "kernel/display/display_runtime.hpp"

#include "kernel/boot/limine_support.hpp"
#include "kernel/display/debug_overlay.hpp"
#include "kernel/display/display_palette.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/gui_panel.hpp"
#include "kernel/display/gui_surface.hpp"

namespace
{

namespace debug_overlay = kernel::display::debug_overlay;
namespace display = kernel::display;
namespace gui_panel = kernel::display::gui_panel;

struct DisplayRuntimeState
{
    display::Surface surface;
    display::DisplayTargetRegistry targets;
    display::AppSurfaceRegistry app_surfaces;
    display::GuiSurfaceRegistry gui_surfaces;
    display::AppSurface terminal_app_surface;
    display::Color terminal_foreground;
    display::Color terminal_background;
    display::HitTestResult pointer_target;
};

DisplayRuntimeState g_state;

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
                                      gui_panel::Config panel_config,
                                      uint64_t terminal_cell_width,
                                      uint64_t terminal_cell_height)
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
    if (app_bounds.width < terminal_cell_width || app_bounds.height < terminal_cell_height)
    {
        return full_bounds;
    }

    return app_bounds;
}

} // namespace

namespace kernel::display::runtime
{

bool ready()
{
    return g_state.surface.ready() && g_state.terminal_app_surface.valid() &&
           g_state.targets.active_target().valid();
}

bool init(
    uint64_t terminal_cell_width,
    uint64_t terminal_cell_height,
    display::compositor::LayerRepaintCallback terminal_repaint_callback)
{
    if (terminal_repaint_callback == nullptr)
    {
        return false;
    }

    const auto * response = boot::framebuffer();
    if (response == nullptr || response->framebuffer_count == 0)
    {
        return false;
    }

    auto * framebuffer = response->framebuffers[0];
    if (framebuffer == nullptr || framebuffer->bpp != 32 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        framebuffer->width < terminal_cell_width ||
        framebuffer->height < terminal_cell_height)
    {
        return false;
    }

    g_state = {};
    g_state.surface = display::Surface(framebuffer->address,
                                       framebuffer->width,
                                       framebuffer->height,
                                       framebuffer->pitch);
    display::compositor::init(framebuffer_bounds(*framebuffer));

    const gui_panel::Config panel_config = gui_panel::default_config();
    const display::Rect panel_bounds =
        gui_panel::bounds_for(framebuffer->width, framebuffer->height, panel_config);
    display::Rect active_panel_bounds;
    const display::GuiSurface panel =
        gui_panel::make_surface(framebuffer->width,
                                framebuffer->height,
                                gui_panel::kGuiSurfaceId,
                                panel_config);
    constexpr display::DisplayPalette palette = display::default_display_palette();
    if (g_state.gui_surfaces.register_surface(panel))
    {
        const display::GuiSurface * registered_panel =
            g_state.gui_surfaces.find(gui_panel::kGuiSurfaceId);
        if (registered_panel != nullptr &&
            g_state.targets.register_target(registered_panel->display_target()))
        {
            const bool panel_layer_registered = display::compositor::register_layer({
                display::LayerKind::DesktopPanel,
                registered_panel->display_surface_id,
                framebuffer_bounds(*framebuffer),
                true,
            });

            if (panel_layer_registered)
            {
                const display::Color panel_border{pack_rgb(*framebuffer, palette.panel_border)};
                const display::Color panel_background{
                    pack_rgb(*framebuffer, palette.desktop_background)};
                const display::Color panel_foreground{
                    pack_rgb(*framebuffer, palette.panel_foreground)};
                if (gui_panel::init(g_state.surface,
                                    *registered_panel,
                                    panel_border,
                                    panel_background,
                                    panel_foreground,
                                    panel_config))
                {
                    active_panel_bounds = panel_bounds;
                }
            }
        }
    }

    g_state.terminal_app_surface =
        display::make_app_surface(display::kTerminalAppSurfaceId,
                                  terminal_app_bounds_for(*framebuffer,
                                                          active_panel_bounds,
                                                          panel_config,
                                                          terminal_cell_width,
                                                          terminal_cell_height),
                                  true,
                                  true);
    if (!g_state.app_surfaces.register_surface(g_state.terminal_app_surface))
    {
        return false;
    }

    const display::AppSurface * registered_app =
        g_state.app_surfaces.find(display::kTerminalAppSurfaceId);
    if (registered_app == nullptr ||
        !g_state.targets.register_target(registered_app->display_target()) ||
        !g_state.targets.set_active(registered_app->display_surface_id) ||
        !g_state.targets.set_focused(registered_app->display_surface_id))
    {
        return false;
    }

    if (!g_state.app_surfaces.set_focused(display::kTerminalAppSurfaceId))
    {
        return false;
    }

    registered_app = g_state.app_surfaces.find(display::kTerminalAppSurfaceId);
    if (registered_app == nullptr)
    {
        return false;
    }

    g_state.terminal_app_surface = *registered_app;
    if (!display::compositor::register_layer(registered_app->layer()) ||
        !display::compositor::register_layer_repaint_callback(display::LayerKind::AppSurface,
                                                              terminal_repaint_callback))
    {
        return false;
    }

    const display::Rect overlay_bounds =
        debug_overlay::bounds_for(framebuffer->width, framebuffer->height);
    if (!overlay_bounds.empty())
    {
        const bool overlay_registered = g_state.targets.register_target({
            debug_overlay::kSurfaceId,
            display::DisplayTargetKind::DebugOverlay,
            overlay_bounds,
            false,
            false,
        });
        const display::SurfaceDescriptor * overlay_target =
            g_state.targets.find(debug_overlay::kSurfaceId);
        if (overlay_registered && overlay_target != nullptr)
        {
            const bool overlay_layer_registered = display::compositor::register_layer({
                display::LayerKind::DebugOverlay,
                debug_overlay::kSurfaceId,
                overlay_bounds,
                true,
            });
            if (overlay_layer_registered)
            {
                const display::Color overlay_foreground{
                    pack_rgb(*framebuffer, palette.debug_overlay_foreground)};
                const display::Color overlay_background{
                    pack_rgb(*framebuffer, palette.debug_overlay_background)};
                (void)debug_overlay::init(g_state.surface,
                                          *overlay_target,
                                          overlay_foreground,
                                          overlay_background);
            }
        }
    }

    g_state.terminal_foreground = {pack_rgb(*framebuffer, palette.terminal_foreground)};
    g_state.terminal_background = {pack_rgb(*framebuffer, palette.terminal_background)};

    return true;
}

TerminalAppConfig terminal_app_config()
{
    return {
        &g_state.surface,
        g_state.terminal_app_surface,
        g_state.terminal_foreground,
        g_state.terminal_background,
    };
}

HitTestResult pointer_target()
{
    return g_state.pointer_target;
}

void refresh_desktop()
{
    gui_panel::refresh_now();
}

void repaint_layers_above_terminal_app(Rect rect)
{
    display::compositor::repaint_layers_above(display::LayerKind::AppSurface, rect);
}

void update_pointer_target(uint64_t x, uint64_t y)
{
    if (!ready())
    {
        g_state.pointer_target = {};
        return;
    }

    g_state.pointer_target =
        display::hit_test(g_state.targets, g_state.app_surfaces, g_state.gui_surfaces, x, y);
}

} // namespace kernel::display::runtime
