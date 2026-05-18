#include "kernel/display/display_runtime.hpp"

#include "debug_overlay_runtime.hpp"
#include "desktop_background_runtime.hpp"
#include "display_runtime_app.hpp"
#include "gui_panel_runtime.hpp"

#include "kernel/boot/limine_support.hpp"
#include "kernel/display/app_layout.hpp"
#include "kernel/display/app_surface_host.hpp"
#include "kernel/display/composited_surface.hpp"
#include "kernel/display/display_frame.hpp"
#include "kernel/display/display_palette.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/framebuffer_presenter.hpp"
#include "kernel/display/gui_surface.hpp"
#include "kernel/display/scene_buffer.hpp"
#include "kernel/display/window_chrome.hpp"
#include "kernel/display/window_interaction.hpp"
#include "kernel/memory/heap.hpp"

#ifndef OS_LAB_TERMINAL_WINDOW_CHROME
#define OS_LAB_TERMINAL_WINDOW_CHROME 0
#endif

#ifndef OS_LAB_TERMINAL_WINDOW_INTERACTION
#define OS_LAB_TERMINAL_WINDOW_INTERACTION 0
#endif

namespace
{

namespace debug_overlay = kernel::display::debug_overlay;
namespace desktop_background = kernel::display::desktop_background;
namespace display = kernel::display;
namespace gui_panel = kernel::display::gui_panel;

enum class TerminalWindowInteractionMode
{
    None,
    Resize,
    Close,
};

struct TerminalWindowInteractionState
{
    bool previous_primary_down = false;
    TerminalWindowInteractionMode mode = TerminalWindowInteractionMode::None;
    display::WindowResizeDrag resize_drag;

    void reset()
    {
        mode = TerminalWindowInteractionMode::None;
        resize_drag = {};
    }
};

struct DisplayRuntimeState
{
    display::Surface surface;
    display::SceneBuffer scene;
    display::FramebufferPresenter presenter;
    display::DisplayFrame frame;
    display::LayerDamageAccumulator app_surface_damage;
    display::DisplayRuntimeStats stats;
    display::DisplayTargetRegistry targets;
    display::AppSurfaceRegistry app_surfaces;
    display::AppSurfaceHost app_host;
    display::GuiSurfaceRegistry gui_surfaces;
    display::AppSurface primary_app_surface;
    display::Color app_foreground;
    display::Color app_background;
    display::HitTestResult pointer_target;
    TerminalWindowInteractionState terminal_window_interaction;
    display::runtime::AppSurfaceResizeCallback app_resize_callback = nullptr;
    display::runtime::AppSurfaceStateCallback app_state_callback = nullptr;
    uint32_t * scene_memory = nullptr;
    size_t scene_bytes = 0;
    uint64_t terminal_cell_width = 0;
    uint64_t terminal_cell_height = 0;
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

display::Rect framebuffer_bounds(const limine_framebuffer & framebuffer)
{
    return {0, 0, framebuffer.width, framebuffer.height};
}

display::WindowFrameConfig terminal_frame_config()
{
    return display::terminal_window_frame_config(OS_LAB_TERMINAL_WINDOW_CHROME != 0);
}

bool terminal_window_interaction_enabled()
{
    return OS_LAB_TERMINAL_WINDOW_CHROME != 0 && OS_LAB_TERMINAL_WINDOW_INTERACTION != 0;
}

display::Rect debug_overlay_avoid_bounds(display::Rect app_bounds)
{
    const display::WindowFrameMetrics metrics =
        display::WindowChrome::metrics_for(app_bounds, terminal_frame_config());
    return metrics.visible ? metrics.title_bar_bounds : display::Rect{};
}

display::Rect primary_app_bounds_for(const limine_framebuffer & framebuffer,
                                     display::Rect panel_bounds,
                                     gui_panel::Config panel_config,
                                     uint64_t terminal_cell_width,
                                     uint64_t terminal_cell_height)
{
    return display::DesktopAppLayout::primary_app_bounds_for({
        framebuffer_bounds(framebuffer),
        panel_bounds,
        panel_config.visible,
        panel_config.margin,
        terminal_cell_height,
        terminal_cell_width,
        terminal_cell_height,
    });
}

bool reset_display_runtime_state(const limine_framebuffer & framebuffer)
{
    uint32_t * old_scene_memory = g_state.scene_memory;
    g_state = {};
    if (old_scene_memory != nullptr)
    {
        if (!kernel::memory::heap::free(old_scene_memory))
        {
            return false;
        }
    }

    g_state.surface = display::Surface(framebuffer.address,
                                       framebuffer.width,
                                       framebuffer.height,
                                       framebuffer.pitch);
    size_t scene_bytes = 0;
    const display::Rect bounds = framebuffer_bounds(framebuffer);
    if (!display::backing_surface_required_bytes(bounds, scene_bytes))
    {
        return false;
    }

    void * scene_memory = kernel::memory::heap::allocate(scene_bytes, alignof(uint32_t));
    if (scene_memory == nullptr)
    {
        return false;
    }

    g_state.scene_memory = static_cast<uint32_t *>(scene_memory);
    g_state.scene_bytes = scene_bytes;
    g_state.scene = display::SceneBuffer(g_state.scene_memory, bounds, framebuffer.width);
    g_state.presenter.reset(g_state.surface, g_state.scene);
    display::compositor::init(framebuffer_bounds(framebuffer));
    display::compositor::set_scene_buffer(g_state.scene);
    display::compositor::set_presenter(g_state.presenter);
    g_state.frame.reset(bounds);
    g_state.app_surface_damage.reset(bounds);
    g_state.app_host.reset(g_state.app_surfaces, g_state.targets);
    g_state.stats = {};
    return true;
}

bool init_desktop_background_layer(const limine_framebuffer & framebuffer,
                                   display::DisplayPalette palette)
{
    return desktop_background::init(framebuffer_bounds(framebuffer),
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

    if (!gui_panel::init(*registered_panel,
                         color_for(framebuffer, palette.panel_border),
                         color_for(framebuffer, palette.desktop_background),
                         color_for(framebuffer, palette.panel_foreground),
                         panel_config))
    {
        return {};
    }

    return panel_bounds;
}

bool init_primary_app_layer(const limine_framebuffer & framebuffer,
                            display::DisplayPalette palette,
                            gui_panel::Config panel_config,
                            display::Rect active_panel_bounds,
                            uint64_t terminal_cell_width,
                            uint64_t terminal_cell_height)
{
    g_state.primary_app_surface =
        display::make_app_surface(display::kTerminalAppSurfaceId,
                                  primary_app_bounds_for(framebuffer,
                                                         active_panel_bounds,
                                                         panel_config,
                                                         terminal_cell_width,
                                                         terminal_cell_height),
                                  true,
                                  true);
    if (!g_state.app_surfaces.register_surface(g_state.primary_app_surface))
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

    g_state.primary_app_surface = *registered_app;
    g_state.app_surface_damage.reset(g_state.primary_app_surface.bounds);
    g_state.app_foreground = color_for(framebuffer, palette.terminal_foreground);
    g_state.app_background = color_for(framebuffer, palette.terminal_background);

    if (!display::compositor::register_surface(registered_app->composited_surface()))
    {
        return false;
    }

    const display::CompositedSurfaceDescriptor caret_surface =
        display::make_composited_surface(display::kTerminalCaretLayerSurfaceId,
                                         display::CompositedSurfaceRole::TextCaret,
                                         registered_app->bounds);
    return display::compositor::register_surface(caret_surface);
}

display::CompositedSurfaceDescriptor text_caret_surface_for(display::Rect app_bounds,
                                                            bool app_visible)
{
    return display::make_composited_surface(display::kTerminalCaretLayerSurfaceId,
                                            display::CompositedSurfaceRole::TextCaret,
                                            app_bounds,
                                            app_visible,
                                            false,
                                            false);
}

bool sync_primary_app_compositor_surface(display::AppSurface surface,
                                         display::Rect previous_bounds)
{
    const display::Rect layer_bounds = surface.closed() ? previous_bounds : surface.bounds;
    const display::CompositedSurfaceDescriptor app_surface =
        surface.closed() ? display::make_composited_surface(surface.display_surface_id,
                                                            display::CompositedSurfaceRole::App,
                                                            layer_bounds,
                                                            false,
                                                            false,
                                                            false)
                         : surface.composited_surface();
    if (!display::compositor::update_surface(app_surface))
    {
        return false;
    }

    return display::compositor::update_surface(text_caret_surface_for(layer_bounds,
                                                                      !surface.closed() &&
                                                                          surface.visible()));
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

    if (!debug_overlay::init(*overlay_target,
                             color_for(framebuffer, palette.debug_overlay_foreground),
                             color_for(framebuffer, palette.debug_overlay_background)))
    {
        return;
    }
}

display::WindowResizeConstraints terminal_resize_constraints()
{
    return {
        g_state.scene.bounds(),
        g_state.terminal_cell_width,
        g_state.terminal_cell_height,
        terminal_frame_config(),
    };
}

} // namespace

namespace kernel::display::runtime
{

bool ready()
{
    return g_state.surface.ready() && g_state.scene.ready() && g_state.presenter.ready() &&
           g_state.primary_app_surface.valid() && !g_state.primary_app_surface.closed();
}

bool init(uint64_t terminal_cell_width, uint64_t terminal_cell_height)
{
    const limine_framebuffer * framebuffer =
        select_usable_framebuffer(terminal_cell_width, terminal_cell_height);
    if (framebuffer == nullptr)
    {
        return false;
    }

    if (!reset_display_runtime_state(*framebuffer))
    {
        return false;
    }

    const gui_panel::Config panel_config = gui_panel::default_config();
    constexpr display::DisplayPalette palette = display::default_display_palette();
    g_state.terminal_cell_width = terminal_cell_width;
    g_state.terminal_cell_height = terminal_cell_height;
    if (!init_desktop_background_layer(*framebuffer, palette))
    {
        return false;
    }

    const display::Rect active_panel_bounds =
        init_optional_gui_panel_layer(*framebuffer, palette, panel_config);
    if (!init_primary_app_layer(*framebuffer,
                                palette,
                                panel_config,
                                active_panel_bounds,
                                terminal_cell_width,
                                terminal_cell_height))
    {
        return false;
    }

    init_optional_debug_overlay_layer(*framebuffer,
                                      palette,
                                      debug_overlay::bounds_for(
                                          framebuffer->width,
                                          framebuffer->height,
                                          debug_overlay_avoid_bounds(g_state.primary_app_surface.bounds)));

    return true;
}

AppSurfaceHostConfig primary_app_config()
{
    return {
        g_state.primary_app_surface,
        g_state.app_foreground,
        g_state.app_background,
    };
}

bool restore_primary_app_surface(display::AppSurface previous)
{
    if (!g_state.app_host.restore_surface(previous))
    {
        return false;
    }

    g_state.primary_app_surface = previous;
    g_state.app_surface_damage.reset(previous.bounds);
    return sync_primary_app_compositor_surface(previous, previous.bounds);
}

bool commit_primary_app_mutation(display::AppSurfaceMutation mutation, bool resize_terminal_app)
{
    if (!sync_primary_app_compositor_surface(mutation.current, mutation.previous.bounds))
    {
        restore_primary_app_surface(mutation.previous);
        return false;
    }

    g_state.primary_app_surface = mutation.current;
    g_state.app_surface_damage.reset(mutation.current.closed() ? mutation.previous.bounds
                                                               : mutation.current.bounds);

    if (resize_terminal_app)
    {
        if (g_state.app_resize_callback == nullptr ||
            !g_state.app_resize_callback(mutation.current))
        {
            restore_primary_app_surface(mutation.previous);
            return false;
        }
    }
    else if (g_state.app_state_callback != nullptr)
    {
        g_state.app_state_callback(mutation.current);
    }

    if (!mutation.repaint_bounds.empty())
    {
        display::compositor::repaint_layers_from(display::LayerKind::DesktopBackground,
                                                 mutation.repaint_bounds);
    }

    return true;
}

bool resize_app_surface(AppSurfaceId id, Rect bounds)
{
    if (!ready() || id != display::kTerminalAppSurfaceId || g_state.app_resize_callback == nullptr)
    {
        return false;
    }

    display::AppSurfaceMutation mutation;
    if (!g_state.app_host.resize_surface(id, bounds, mutation))
    {
        return false;
    }

    return commit_primary_app_mutation(mutation, true);
}

bool set_app_surface_visible(AppSurfaceId id, bool visible)
{
    if (!ready() || id != display::kTerminalAppSurfaceId)
    {
        return false;
    }

    display::AppSurfaceMutation mutation;
    if (!g_state.app_host.set_visible(id, visible, mutation))
    {
        return false;
    }

    return commit_primary_app_mutation(mutation, false);
}

bool clear_app_surface_focus(AppSurfaceId id)
{
    if (!ready() || id != display::kTerminalAppSurfaceId)
    {
        return false;
    }

    display::AppSurfaceMutation mutation;
    if (!g_state.app_host.clear_focus(id, mutation))
    {
        return false;
    }

    return commit_primary_app_mutation(mutation, false);
}

bool focus_app_surface(AppSurfaceId id)
{
    if (!ready() || id != display::kTerminalAppSurfaceId)
    {
        return false;
    }

    display::AppSurfaceMutation mutation;
    if (!g_state.app_host.focus_surface(id, mutation))
    {
        return false;
    }

    return commit_primary_app_mutation(mutation, false);
}

bool close_app_surface(AppSurfaceId id)
{
    if (!ready() || id != display::kTerminalAppSurfaceId)
    {
        return false;
    }

    display::AppSurfaceMutation mutation;
    if (!g_state.app_host.close_surface(id, mutation))
    {
        return false;
    }

    return commit_primary_app_mutation(mutation, false);
}

HitTestResult pointer_target()
{
    return g_state.pointer_target;
}

DisplayPipelineStats stats()
{
    return {
        g_state.frame.stats(),
        g_state.presenter.stats(),
        display::compositor::stats(),
        g_state.stats,
    };
}

void reset_stats()
{
    g_state.frame.reset_stats();
    g_state.presenter.reset_stats();
    display::compositor::reset_stats();
    g_state.stats = {};
}

void begin_frame()
{
    if (ready())
    {
        g_state.frame.begin();
    }
}

void present_scene_operations(const display::PresentOperationList & operations)
{
    display::compositor::present_scene_operations(operations);
}

void submit_present_operations_to_frame(const display::PresentOperationList & operations)
{
    const DisplayFrameSubmit submit = g_state.frame.submit(operations);
    if (submit.immediate)
    {
        present_scene_operations(submit.present_operations);
    }
}

void flush_app_surface_damage_into_frame()
{
    if (g_state.app_surface_damage.empty())
    {
        return;
    }

    const display::PresentOperationList present_operations =
        display::compositor::update_scene_from_layer_damage(display::LayerKind::AppSurface,
                                                            g_state.app_surface_damage.flush());
    submit_present_operations_to_frame(present_operations);
}

void end_frame()
{
    if (!ready())
    {
        return;
    }

    if (g_state.frame.depth() == 1)
    {
        flush_app_surface_damage_into_frame();
    }

    const DisplayFrameFlush flush = g_state.frame.end();
    if (flush.outermost_frame_ended)
    {
        present_scene_operations(flush.present_operations);
    }
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

void submit_app_surface_damage(FrameDamage damage)
{
    if (g_state.frame.in_frame())
    {
        g_state.app_surface_damage.record(damage);
        return;
    }

    const display::PresentOperationList present_operations =
        display::compositor::update_scene_from_layer_damage(display::LayerKind::AppSurface, damage);
    submit_present_operations_to_frame(present_operations);
}

bool register_app_surface_resize_callback(AppSurfaceResizeCallback callback)
{
    if (callback == nullptr)
    {
        return false;
    }

    g_state.app_resize_callback = callback;
    return true;
}

bool register_app_surface_state_callback(AppSurfaceStateCallback callback)
{
    if (callback == nullptr)
    {
        return false;
    }

    g_state.app_state_callback = callback;
    return true;
}

bool register_app_surface_pixel_source(compositor::LayerPixelCallback callback)
{
    return display::compositor::register_layer_pixel_callback(display::LayerKind::AppSurface, callback);
}

bool register_app_surface_row_source(compositor::LayerRowCallback callback)
{
    return display::compositor::register_layer_row_callback(display::LayerKind::AppSurface, callback);
}

bool register_app_surface_scroll_composition(LayerScrollComposition composition)
{
    return display::compositor::register_layer_scroll_composition(display::LayerKind::AppSurface,
                                                                  composition);
}

bool register_text_caret(compositor::LayerPixelCallback pixel_callback,
                         compositor::LayerBoundsCallback bounds_callback)
{
    return display::compositor::register_layer_pixel_callback(display::LayerKind::TerminalCaret,
                                                              pixel_callback) &&
           display::compositor::register_layer_bounds_callback(display::LayerKind::TerminalCaret,
                                                               bounds_callback);
}

void update_pointer_target(uint64_t x, uint64_t y)
{
    if (!ready())
    {
        g_state.pointer_target = {};
        return;
    }

    g_state.pointer_target = display::hit_test(g_state.targets,
                                               g_state.app_surfaces,
                                               g_state.gui_surfaces,
                                               x,
                                               y,
                                               terminal_frame_config());
}

TerminalWindowInteractionResult handle_terminal_window_pointer(uint64_t x,
                                                               uint64_t y,
                                                               bool primary_down)
{
    TerminalWindowInteractionResult result;
    if (!ready() || !terminal_window_interaction_enabled())
    {
        g_state.terminal_window_interaction.previous_primary_down = primary_down;
        return result;
    }

    const bool pressed = primary_down && !g_state.terminal_window_interaction.previous_primary_down;
    const bool released = !primary_down && g_state.terminal_window_interaction.previous_primary_down;

    if (pressed && g_state.pointer_target.target_kind == display::DisplayTargetKind::AppSurface &&
        g_state.pointer_target.app_surface_id == display::kTerminalAppSurfaceId)
    {
        if (g_state.pointer_target.app_chrome_region == display::WindowChromeHitRegion::ResizeHandle)
        {
            g_state.terminal_window_interaction.mode = TerminalWindowInteractionMode::Resize;
            g_state.terminal_window_interaction.resize_drag =
                display::WindowResizeDrag::begin(g_state.primary_app_surface.bounds,
                                                 x,
                                                 y,
                                                 terminal_resize_constraints());
            result.handled = true;
        }
        else if (g_state.pointer_target.app_chrome_region ==
                 display::WindowChromeHitRegion::CloseButton)
        {
            g_state.terminal_window_interaction.mode = TerminalWindowInteractionMode::Close;
            result.handled = true;
        }
    }

    if (primary_down && g_state.terminal_window_interaction.mode != TerminalWindowInteractionMode::None)
    {
        result.handled = true;
    }

    if (released)
    {
        if (g_state.terminal_window_interaction.mode == TerminalWindowInteractionMode::Resize)
        {
            const display::Rect resized_bounds =
                g_state.terminal_window_interaction.resize_drag.bounds_for(x, y);
            result.handled = true;
            if (!resized_bounds.empty())
            {
                const bool resized =
                    resize_app_surface(display::kTerminalAppSurfaceId, resized_bounds);
                if (resized)
                {
                    update_pointer_target(x, y);
                }
            }
        }
        else if (g_state.terminal_window_interaction.mode == TerminalWindowInteractionMode::Close)
        {
            result.handled = true;
            if (g_state.pointer_target.target_kind == display::DisplayTargetKind::AppSurface &&
                g_state.pointer_target.app_surface_id == display::kTerminalAppSurfaceId &&
                g_state.pointer_target.app_chrome_region == display::WindowChromeHitRegion::CloseButton)
            {
                result.clear_keyboard_focus =
                    set_app_surface_visible(display::kTerminalAppSurfaceId, false);
                update_pointer_target(x, y);
            }
        }

        g_state.terminal_window_interaction.reset();
    }

    g_state.terminal_window_interaction.previous_primary_down = primary_down;
    return result;
}

} // namespace kernel::display::runtime
