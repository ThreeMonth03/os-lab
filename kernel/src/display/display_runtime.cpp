#include "kernel/display/display_runtime.hpp"

#include "debug_overlay_runtime.hpp"
#include "desktop_background_runtime.hpp"
#include "display_runtime_terminal.hpp"
#include "gui_panel_runtime.hpp"

#include "kernel/boot/limine_support.hpp"
#include "kernel/display/composited_surface.hpp"
#include "kernel/display/display_palette.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/gui_surface.hpp"

namespace
{

namespace debug_overlay = kernel::display::debug_overlay;
namespace desktop_background = kernel::display::desktop_background;
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

const limine_framebuffer * select_usable_framebuffer(uint64_t terminal_cell_width,
                                                     uint64_t terminal_cell_height)
{
    const auto * response = kernel::boot::framebuffer();
    if (response == nullptr || response->framebuffer_count == 0)
    {
        return nullptr;
    }

    const auto * framebuffer = response->framebuffers[0];
    if (framebuffer == nullptr || framebuffer->bpp != 32 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB ||
        framebuffer->width < terminal_cell_width ||
        framebuffer->height < terminal_cell_height)
    {
        return nullptr;
    }

    return framebuffer;
}

uint32_t pack_framebuffer_rgb(const limine_framebuffer & framebuffer, display::RgbColor color)
{
    return (static_cast<uint32_t>(color.red) << framebuffer.red_mask_shift) |
           (static_cast<uint32_t>(color.green) << framebuffer.green_mask_shift) |
           (static_cast<uint32_t>(color.blue) << framebuffer.blue_mask_shift);
}

display::Color color_for(const limine_framebuffer & framebuffer, display::RgbColor color)
{
    return {pack_framebuffer_rgb(framebuffer, color)};
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

void reset_display_runtime_state(const limine_framebuffer & framebuffer)
{
    g_state = {};
    g_state.surface = display::Surface(framebuffer.address,
                                       framebuffer.width,
                                       framebuffer.height,
                                       framebuffer.pitch);
    display::compositor::init(framebuffer_bounds(framebuffer));
    display::compositor::set_framebuffer_surface(g_state.surface);
}

bool init_desktop_background_layer(const limine_framebuffer & framebuffer,
                                   display::DisplayPalette palette)
{
    return desktop_background::init(g_state.surface,
                                    framebuffer_bounds(framebuffer),
                                    desktop_background::solid_background(
                                        color_for(framebuffer, palette.desktop_background)));
}

display::Rect init_optional_gui_panel_layer(const limine_framebuffer & framebuffer,
                                            display::DisplayPalette palette,
                                            gui_panel::Config panel_config)
{
    const display::Rect panel_bounds =
        gui_panel::bounds_for(framebuffer.width, framebuffer.height, panel_config);
    const display::GuiSurface panel =
        gui_panel::make_surface(framebuffer.width,
                                framebuffer.height,
                                gui_panel::kGuiSurfaceId,
                                panel_config);

    if (!g_state.gui_surfaces.register_surface(panel))
    {
        return {};
    }

    const display::GuiSurface * registered_panel =
        g_state.gui_surfaces.find(gui_panel::kGuiSurfaceId);
    if (registered_panel == nullptr ||
        !g_state.targets.register_target(registered_panel->display_target()))
    {
        return {};
    }

    if (!display::compositor::register_surface(registered_panel->composited_surface()))
    {
        return {};
    }

    if (!gui_panel::init(g_state.surface,
                         *registered_panel,
                         color_for(framebuffer, palette.panel_border),
                         color_for(framebuffer, palette.desktop_background),
                         color_for(framebuffer, palette.panel_foreground),
                         panel_config))
    {
        return {};
    }

    return panel_bounds;
}

bool init_terminal_app_layer(const limine_framebuffer & framebuffer,
                             display::DisplayPalette palette,
                             gui_panel::Config panel_config,
                             display::Rect active_panel_bounds,
                             uint64_t terminal_cell_width,
                             uint64_t terminal_cell_height,
                             display::compositor::LayerRepaintCallback repaint_callback)
{
    g_state.terminal_app_surface =
        display::make_app_surface(display::kTerminalAppSurfaceId,
                                  terminal_app_bounds_for(framebuffer,
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
    g_state.terminal_foreground = color_for(framebuffer, palette.terminal_foreground);
    g_state.terminal_background = color_for(framebuffer, palette.terminal_background);

    return display::compositor::register_surface(registered_app->composited_surface()) &&
           display::compositor::register_layer_repaint_callback(display::LayerKind::AppSurface,
                                                                repaint_callback);
}

void init_optional_debug_overlay_layer(const limine_framebuffer & framebuffer,
                                       display::DisplayPalette palette,
                                       display::Rect overlay_bounds)
{
    if (overlay_bounds.empty())
    {
        return;
    }

    const display::CompositedSurfaceDescriptor overlay =
        display::make_composited_surface(debug_overlay::kSurfaceId,
                                         display::CompositedSurfaceRole::Overlay,
                                         overlay_bounds);
    const bool overlay_registered = g_state.targets.register_target(overlay.display_target());
    const display::SurfaceDescriptor * overlay_target =
        g_state.targets.find(debug_overlay::kSurfaceId);
    if (!overlay_registered || overlay_target == nullptr)
    {
        return;
    }

    const bool overlay_layer_registered = display::compositor::register_surface(overlay);
    if (!overlay_layer_registered)
    {
        return;
    }

    if (!debug_overlay::init(g_state.surface,
                             *overlay_target,
                             color_for(framebuffer, palette.debug_overlay_foreground),
                             color_for(framebuffer, palette.debug_overlay_background)))
    {
        return;
    }
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

    const limine_framebuffer * framebuffer =
        select_usable_framebuffer(terminal_cell_width, terminal_cell_height);
    if (framebuffer == nullptr)
    {
        return false;
    }

    reset_display_runtime_state(*framebuffer);

    const gui_panel::Config panel_config = gui_panel::default_config();
    constexpr display::DisplayPalette palette = display::default_display_palette();
    if (!init_desktop_background_layer(*framebuffer, palette))
    {
        return false;
    }

    const display::Rect active_panel_bounds =
        init_optional_gui_panel_layer(*framebuffer, palette, panel_config);
    if (!init_terminal_app_layer(*framebuffer,
                                 palette,
                                 panel_config,
                                 active_panel_bounds,
                                 terminal_cell_width,
                                 terminal_cell_height,
                                 terminal_repaint_callback))
    {
        return false;
    }

    init_optional_debug_overlay_layer(
        *framebuffer,
        palette,
        debug_overlay::bounds_for(framebuffer->width, framebuffer->height));

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
    if (!g_state.surface.ready())
    {
        return;
    }

    display::compositor::repaint_layers_from(display::LayerKind::DesktopBackground,
                                             {0, 0, g_state.surface.width(), g_state.surface.height()});
}

void refresh_debug_overlay_if_due()
{
    debug_overlay::refresh_if_due();
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
