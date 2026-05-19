#include "kernel/display/display_runtime.hpp"

#include "debug_overlay_runtime.hpp"
#include "desktop_bar_runtime.hpp"
#include "desktop_background_runtime.hpp"
#include "display_runtime_app.hpp"

#include "kernel/boot/limine_support.hpp"
#include "kernel/display/app_layout.hpp"
#include "kernel/display/app_surface_host.hpp"
#include "kernel/display/app_window.hpp"
#include "kernel/display/composited_surface.hpp"
#include "kernel/display/desktop_shell.hpp"
#include "kernel/display/display_frame.hpp"
#include "kernel/display/display_palette.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/framebuffer_presenter.hpp"
#include "kernel/display/gui_surface.hpp"
#include "kernel/display/pointer_cursor_shape.hpp"
#include "kernel/display/scene_buffer.hpp"
#include "kernel/display/window_chrome.hpp"
#include "kernel/display/window_interaction.hpp"
#include "kernel/display/window_manager.hpp"
#include "kernel/display/window_session.hpp"
#include "kernel/display/window_stack.hpp"
#include "kernel/memory/heap.hpp"

#ifndef OS_LAB_TERMINAL_WINDOW_CHROME
#define OS_LAB_TERMINAL_WINDOW_CHROME 0
#endif

#ifndef OS_LAB_TERMINAL_WINDOW_INTERACTION
#define OS_LAB_TERMINAL_WINDOW_INTERACTION 0
#endif

#ifndef OS_LAB_DESKTOP_BAR_DEBUG_ACTIONS
#define OS_LAB_DESKTOP_BAR_DEBUG_ACTIONS 0
#endif

#ifndef OS_LAB_DESKTOP_DEBUG_DUMMY_APP
#define OS_LAB_DESKTOP_DEBUG_DUMMY_APP 0
#endif

namespace
{

namespace debug_overlay = kernel::display::debug_overlay;
namespace desktop_bar = kernel::display::desktop_bar;
namespace desktop_background = kernel::display::desktop_background;
namespace display = kernel::display;

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
    display::WindowSessionRegistry window_sessions;
    display::WindowSessionHost window_host;
    display::WindowStack window_stack;
    display::WindowManager window_manager;
    display::AppWindowRegistry app_windows;
    display::GuiSurfaceRegistry gui_surfaces;
    display::WindowSession primary_window_session;
    display::AppSurface primary_app_surface;
    display::WindowSession dummy_window_session;
    display::AppSurface dummy_app_surface;
    display::Rect app_work_area;
    display::Rect app_layer_bounds;
    display::Color app_foreground;
    display::Color app_background;
    display::HitTestResult pointer_target;
    display::WindowInteractionController terminal_window_interaction;
    display::WindowSessionId active_window_interaction_session_id =
        display::kInvalidWindowSessionId;
    display::AppSurfaceId active_window_interaction_app_surface_id =
        display::kInvalidAppSurfaceId;
    display::PointerCursorShape pointer_cursor_shape = display::PointerCursorShape::Arrow;
    display::runtime::AppSurfaceResizeCallback app_resize_callback = nullptr;
    display::runtime::AppSurfaceStateCallback app_state_callback = nullptr;
    display::compositor::LayerPixelCallback terminal_app_pixel_callback = nullptr;
    display::compositor::LayerRowCallback terminal_app_row_callback = nullptr;
    desktop_bar::DesktopShellAction pressed_desktop_shell_action =
        desktop_bar::DesktopShellAction::None;
    uint32_t * scene_memory = nullptr;
    size_t scene_bytes = 0;
    uint64_t terminal_cell_width = 0;
    uint64_t terminal_cell_height = 0;
};

DisplayRuntimeState g_state;

struct DisplayFeatureConfig
{
    bool terminal_window_chrome = false;
    bool terminal_window_interaction = false;
    bool desktop_bar_debug_actions = false;
    bool desktop_debug_dummy_app = false;
};

constexpr DisplayFeatureConfig display_feature_config()
{
    return {
        OS_LAB_TERMINAL_WINDOW_CHROME != 0,
        OS_LAB_TERMINAL_WINDOW_INTERACTION != 0,
        OS_LAB_DESKTOP_BAR_DEBUG_ACTIONS != 0,
        OS_LAB_DESKTOP_DEBUG_DUMMY_APP != 0,
    };
}

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
    return display::terminal_window_frame_config(display_feature_config().terminal_window_chrome);
}

bool terminal_window_interaction_enabled()
{
    const DisplayFeatureConfig config = display_feature_config();
    return config.terminal_window_chrome && config.terminal_window_interaction;
}

bool desktop_bar_debug_actions_enabled()
{
    return display_feature_config().desktop_bar_debug_actions && terminal_window_interaction_enabled();
}

bool desktop_debug_dummy_app_enabled()
{
    const DisplayFeatureConfig config = display_feature_config();
    return config.desktop_debug_dummy_app && config.terminal_window_chrome;
}

desktop_bar::TerminalItemState terminal_item_state_for(display::WindowSession session)
{
    return {
        session.visible(),
        session.focused,
        session.active,
        session.closed(),
    };
}

desktop_bar::WindowItemState dummy_item_state_for(display::WindowSession session)
{
    if (!desktop_debug_dummy_app_enabled() || !session.valid())
    {
        return {};
    }

    return {
        desktop_bar::ItemKind::DummyDebugApp,
        session.visible(),
        session.focused,
        session.active,
        session.closed(),
    };
}

display::WindowSessionId window_session_id_for_app_surface(display::AppSurfaceId id)
{
    switch (id)
    {
    case display::kTerminalAppSurfaceId:
        return display::kTerminalWindowSessionId;
    case display::kDummyDebugAppSurfaceId:
        return display::kDummyDebugWindowSessionId;
    case display::kInvalidAppSurfaceId:
        return display::kInvalidWindowSessionId;
    default:
        return display::kInvalidWindowSessionId;
    }
}

void clear_active_window_interaction_target()
{
    g_state.active_window_interaction_session_id = display::kInvalidWindowSessionId;
    g_state.active_window_interaction_app_surface_id = display::kInvalidAppSurfaceId;
}

bool active_window_interaction_target_valid()
{
    return g_state.active_window_interaction_session_id != display::kInvalidWindowSessionId &&
           g_state.active_window_interaction_app_surface_id != display::kInvalidAppSurfaceId;
}

display::WindowSessionId pointer_window_session_id()
{
    if (g_state.pointer_target.target_kind != display::DisplayTargetKind::AppSurface)
    {
        return display::kInvalidWindowSessionId;
    }
    return window_session_id_for_app_surface(g_state.pointer_target.app_surface_id);
}

display::WindowSessionBounds window_session_bounds_for(display::Rect outer_bounds)
{
    const display::WindowFrameMetrics metrics =
        display::WindowChrome::metrics_for(outer_bounds, terminal_frame_config());
    return {
        outer_bounds,
        metrics.valid() ? metrics.client_bounds : display::Rect{},
    };
}

display::Rect desktop_bar_bounds()
{
    const display::GuiSurface * bar = g_state.gui_surfaces.find(desktop_bar::kGuiSurfaceId);
    return bar == nullptr || !bar->visible ? display::Rect{} : bar->bounds;
}

bool same_rect(display::Rect lhs, display::Rect rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width &&
           lhs.height == rhs.height;
}

void sync_desktop_bar_terminal_item()
{
    desktop_bar::sync_terminal_item_state(terminal_item_state_for(g_state.primary_window_session));
    desktop_bar::sync_dummy_debug_item_state(dummy_item_state_for(g_state.dummy_window_session));
}

void repaint_desktop_bar_if_visible()
{
    const display::Rect bounds = desktop_bar_bounds();
    if (!bounds.empty())
    {
        display::compositor::repaint_layers_from(display::LayerKind::GuiSurface, bounds);
    }
}

void repaint_window_bounds(display::WindowSessionId id)
{
    const display::WindowSession * session = g_state.window_host.find(id);
    if (session != nullptr && !session->bounds.outer.empty())
    {
        display::compositor::repaint_layers_from(display::LayerKind::DesktopBackground,
                                                 session->bounds.outer);
    }
}

bool stack_visual_state_changed(const display::WindowStackEntry & previous,
                                const display::WindowStackEntry & current)
{
    return previous.visible() != current.visible() || previous.focused != current.focused ||
           previous.active != current.active || previous.closed() != current.closed();
}

void repaint_changed_window_visual_states(const display::WindowStack & previous,
                                          const display::WindowStack & current)
{
    for (size_t index = 0; index < previous.size(); ++index)
    {
        const display::WindowStackEntry * previous_entry = previous.at(index);
        if (previous_entry == nullptr)
        {
            continue;
        }

        const display::WindowStackEntry * current_entry = current.find(previous_entry->id);
        if (current_entry != nullptr &&
            stack_visual_state_changed(*previous_entry, *current_entry))
        {
            repaint_window_bounds(previous_entry->id);
        }
    }
}

display::Rect debug_overlay_avoid_bounds(display::Rect app_bounds)
{
    const display::WindowFrameMetrics metrics =
        display::WindowChrome::metrics_for(app_bounds, terminal_frame_config());
    return metrics.visible ? metrics.title_bar_bounds : display::Rect{};
}

display::Rect debug_overlay_app_chrome_avoid_bounds(display::WindowSession session)
{
    if (!session.visible() || session.closed())
    {
        return {};
    }

    return debug_overlay_avoid_bounds(session.bounds.outer);
}

display::Rect desktop_bar_terminal_item_bounds()
{
    const display::GuiSurface * bar = g_state.gui_surfaces.find(desktop_bar::kGuiSurfaceId);
    if (bar == nullptr || !bar->visible)
    {
        return {};
    }

    const desktop_bar::ItemList items =
        desktop_bar::item_list_for(*bar,
                                   desktop_bar::default_config(),
                                   terminal_item_state_for(g_state.primary_window_session),
                                   dummy_item_state_for(g_state.dummy_window_session));
    display::Rect bounds;
    for (size_t index = 0; index < items.count; ++index)
    {
        bounds = display::bounding_rect(bounds, items.items[index].bounds);
    }
    return bounds;
}

display::Rect debug_overlay_bounds_for_current_layout()
{
    const display::Rect desktop_bounds =
        g_state.scene.ready() ? g_state.scene.bounds() : display::Rect{};
    return debug_overlay::desktop_status_bounds_for({
        desktop_bounds,
        debug_overlay_app_chrome_avoid_bounds(g_state.primary_window_session),
        desktop_bar_bounds(),
        desktop_bar_terminal_item_bounds(),
        {},
    });
}

debug_overlay::Config debug_overlay_config_for_bounds(display::Rect bounds)
{
    debug_overlay::Config config;
    if (!display::intersect_rect(bounds, desktop_bar_bounds()).empty())
    {
        config.text_alignment = debug_overlay::StatusTextAlignment::Right;
    }
    return config;
}

void relayout_debug_overlay_if_present()
{
    const display::SurfaceDescriptor * current_target =
        g_state.targets.find(debug_overlay::kSurfaceId);
    if (current_target == nullptr)
    {
        return;
    }

    const display::Rect next_bounds = debug_overlay_bounds_for_current_layout();
    if (next_bounds.empty() || same_rect(current_target->bounds, next_bounds))
    {
        return;
    }

    const display::Rect previous_bounds = current_target->bounds;
    const display::CompositedSurfaceDescriptor overlay =
        display::make_composited_surface(debug_overlay::kSurfaceId,
                                         display::CompositedSurfaceRole::Overlay,
                                         next_bounds);
    const display::SurfaceDescriptor next_target = overlay.display_target();
    if (!display::compositor::update_surface(overlay) ||
        !g_state.targets.update_target(next_target) ||
        !debug_overlay::update_target(next_target, debug_overlay_config_for_bounds(next_bounds)))
    {
        return;
    }

    display::compositor::repaint_layers_from(display::LayerKind::DesktopBackground,
                                             previous_bounds);
    display::compositor::repaint_layers_from(display::LayerKind::DesktopBackground,
                                             next_bounds);
}

display::Rect primary_app_bounds_for(const limine_framebuffer & framebuffer,
                                     display::Rect work_area,
                                     bool system_ui_visible,
                                     uint64_t terminal_cell_width,
                                     uint64_t terminal_cell_height)
{
    return display::DesktopAppLayout::primary_app_bounds_for({
        work_area.empty() ? framebuffer_bounds(framebuffer) : work_area,
        {},
        system_ui_visible,
        0,
        terminal_cell_height,
        terminal_cell_width,
        terminal_cell_height,
    });
}

display::Rect dummy_app_bounds_for(display::Rect work_area)
{
    if (work_area.empty())
    {
        return {};
    }

    const uint64_t width = work_area.width > 480 ? 360 : (work_area.width * 3) / 5;
    const uint64_t height = work_area.height > 360 ? 220 : (work_area.height * 3) / 5;
    if (width < 96 || height < 72)
    {
        return {};
    }

    return {
        work_area.x + work_area.width - width - 24,
        work_area.y + 56,
        width,
        height,
    };
}

display::Rect visible_app_layer_bounds()
{
    display::Rect bounds;
    for (size_t index = 0; index < g_state.window_stack.size(); ++index)
    {
        const display::WindowStackEntry * entry = g_state.window_stack.at(index);
        if (entry == nullptr || !entry->visible())
        {
            continue;
        }

        const display::WindowSession * session = g_state.window_host.find(entry->id);
        if (session == nullptr || !session->visible() || session->closed())
        {
            continue;
        }
        bounds = display::bounding_rect(bounds, session->bounds.outer);
    }

    return bounds;
}

display::CompositedSurfaceDescriptor aggregate_app_composited_surface(
    display::Rect retained_bounds = {})
{
    display::Rect bounds = visible_app_layer_bounds();
    const bool visible = !bounds.empty();
    if (!visible)
    {
        bounds = retained_bounds;
    }
    if (bounds.empty())
    {
        return {};
    }

    return display::make_composited_surface(display::app_surface_display_id_for(
                                                display::kTerminalAppSurfaceId),
                                            display::CompositedSurfaceRole::App,
                                            bounds,
                                            visible,
                                            false,
                                            false);
}

bool sync_aggregate_app_layer(display::Rect retained_bounds = {})
{
    const display::CompositedSurfaceDescriptor app_layer =
        aggregate_app_composited_surface(retained_bounds);
    if (!app_layer.valid() || !display::compositor::update_surface(app_layer))
    {
        return false;
    }
    g_state.app_layer_bounds = app_layer.bounds;
    return true;
}

bool runtime_rect_contains(display::Rect rect, uint64_t x, uint64_t y)
{
    return display::rect_contains(rect, x, y);
}

bool chrome_region_is_border_or_resize(display::WindowChromeHitRegion region)
{
    switch (region)
    {
    case display::WindowChromeHitRegion::Border:
    case display::WindowChromeHitRegion::ResizeLeft:
    case display::WindowChromeHitRegion::ResizeRight:
    case display::WindowChromeHitRegion::ResizeTop:
    case display::WindowChromeHitRegion::ResizeBottom:
    case display::WindowChromeHitRegion::ResizeTopLeft:
    case display::WindowChromeHitRegion::ResizeTopRight:
    case display::WindowChromeHitRegion::ResizeBottomLeft:
    case display::WindowChromeHitRegion::ResizeBottomRight:
        return true;
    case display::WindowChromeHitRegion::None:
    case display::WindowChromeHitRegion::TitleBar:
    case display::WindowChromeHitRegion::CloseButton:
    case display::WindowChromeHitRegion::Content:
        return false;
    }
    return false;
}

display::PixelSample sample_dummy_debug_app_pixel(display::AppSurface surface,
                                                  uint64_t x,
                                                  uint64_t y)
{
    if (!surface.valid() || !surface.visible() ||
        !runtime_rect_contains(surface.bounds, x, y))
    {
        return display::transparent_pixel();
    }

    constexpr display::Color kBackground{0x00131924};
    constexpr display::Color kAccent{0x0048c7ff};
    constexpr display::Color kCheckerDark{0x001b2a36};
    constexpr display::Color kCheckerLight{0x00293d4f};
    constexpr display::Color kText{0x00d7f4ff};

    const display::WindowFrameMetrics metrics =
        display::WindowChrome::metrics_for(surface.bounds, terminal_frame_config());
    if (metrics.visible && display::rect_contains(metrics.title_bar_bounds, x, y))
    {
        if ((x + y) % 5 == 0)
        {
            return display::opaque_pixel(kAccent);
        }
        return display::opaque_pixel(kBackground);
    }

    if (metrics.visible && display::WindowChrome::close_button_icon_contains_pixel(metrics, x, y))
    {
        return display::opaque_pixel(kText);
    }

    const bool border =
        metrics.visible &&
        chrome_region_is_border_or_resize(display::WindowChrome::hit_test(metrics, x, y));
    if (border)
    {
        return display::opaque_pixel(surface.active ? kAccent : kText);
    }

    const display::Rect client = metrics.valid() ? metrics.client_bounds : surface.bounds;
    if (!display::rect_contains(client, x, y))
    {
        return display::opaque_pixel(kBackground);
    }

    const uint64_t cell_x = (x - client.x) / 16;
    const uint64_t cell_y = (y - client.y) / 16;
    if (((cell_x + cell_y) % 2) == 0)
    {
        return display::opaque_pixel(kCheckerLight);
    }
    return display::opaque_pixel(kCheckerDark);
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
    g_state.window_host.reset(g_state.window_sessions, g_state.app_host);
    g_state.window_manager.reset(g_state.window_host, g_state.window_stack);
    g_state.stats = {};
    return true;
}

display::PixelSample sample_app_window_pixel(display::WindowSession session,
                                             uint64_t x,
                                             uint64_t y)
{
    if (!session.visible() || session.closed() ||
        !runtime_rect_contains(session.bounds.outer, x, y))
    {
        return display::transparent_pixel();
    }

    switch (session.role)
    {
    case display::WindowSessionRole::Terminal:
        return g_state.terminal_app_pixel_callback == nullptr
                   ? display::transparent_pixel()
                   : g_state.terminal_app_pixel_callback(x, y);
    case display::WindowSessionRole::DummyDebugApp:
        return sample_dummy_debug_app_pixel(session.app_surface(), x, y);
    }
    return display::transparent_pixel();
}

display::PixelSample sample_app_layer_pixel(uint64_t x, uint64_t y)
{
    for (size_t offset = 0; offset < g_state.window_stack.size(); ++offset)
    {
        const size_t index = g_state.window_stack.size() - offset - 1;
        const display::WindowStackEntry * entry = g_state.window_stack.at(index);
        if (entry == nullptr || !entry->visible())
        {
            continue;
        }

        const display::WindowSession * session = g_state.window_host.find(entry->id);
        if (session == nullptr)
        {
            continue;
        }

        const display::PixelSample sample = sample_app_window_pixel(*session, x, y);
        if (sample.opaque())
        {
            return sample;
        }
    }
    return display::transparent_pixel();
}

const uint32_t * app_layer_row_pixels(uint64_t y)
{
    if (g_state.terminal_app_row_callback == nullptr)
    {
        return nullptr;
    }

    const display::WindowSession & terminal = g_state.primary_window_session;
    if (!terminal.visible() || terminal.closed() ||
        !same_rect(g_state.app_layer_bounds, terminal.bounds.outer))
    {
        return nullptr;
    }

    for (size_t index = 0; index < g_state.window_stack.size(); ++index)
    {
        const display::WindowStackEntry * entry = g_state.window_stack.at(index);
        const display::WindowSession * session =
            entry == nullptr ? nullptr : g_state.window_host.find(entry->id);
        if (session == nullptr || !session->visible() ||
            session->role == display::WindowSessionRole::Terminal)
        {
            continue;
        }

        if (y >= session->bounds.outer.y &&
            y < session->bounds.outer.y + session->bounds.outer.height)
        {
            return nullptr;
        }
    }

    return g_state.terminal_app_row_callback(y);
}

bool init_desktop_background_layer(const limine_framebuffer & framebuffer,
                                   display::DisplayPalette palette)
{
    return desktop_background::init(framebuffer_bounds(framebuffer),
                                    desktop_background::solid_background(
                                        color_for(framebuffer, palette.desktop_background)));
}

desktop_bar::Layout init_desktop_bar_layer(const limine_framebuffer & framebuffer,
                                           display::DisplayPalette palette,
                                           desktop_bar::Config bar_config)
{
    const display::Rect desktop_bounds = framebuffer_bounds(framebuffer);
    const desktop_bar::Layout layout = desktop_bar::layout_for(desktop_bounds, bar_config);
    const display::GuiSurface bar =
        desktop_bar::make_surface(desktop_bounds, desktop_bar::kGuiSurfaceId, bar_config);

    if (!g_state.gui_surfaces.register_surface(bar))
    {
        return {};
    }

    const display::GuiSurface * registered_bar =
        g_state.gui_surfaces.find(desktop_bar::kGuiSurfaceId);
    if (registered_bar == nullptr ||
        !g_state.targets.register_target(registered_bar->display_target()))
    {
        return {};
    }

    if (!display::compositor::register_surface(registered_bar->composited_surface()))
    {
        return {};
    }

    if (!desktop_bar::init(*registered_bar,
                           {
                               color_for(framebuffer, palette.panel_border),
                               color_for(framebuffer, palette.terminal_background),
                               color_for(framebuffer, palette.panel_border),
                               color_for(framebuffer, palette.desktop_background),
                               color_for(framebuffer, palette.terminal_background),
                               color_for(framebuffer, palette.terminal_foreground),
                           },
                           bar_config))
    {
        return {};
    }

    return layout;
}

bool init_primary_app_layer(const limine_framebuffer & framebuffer,
                            display::DisplayPalette palette,
                            display::Rect app_work_area,
                            bool system_ui_visible,
                            uint64_t terminal_cell_width,
                            uint64_t terminal_cell_height)
{
    g_state.app_work_area = app_work_area.empty() ? framebuffer_bounds(framebuffer) : app_work_area;
    g_state.primary_app_surface =
        display::make_app_surface(display::kTerminalAppSurfaceId,
                                  primary_app_bounds_for(framebuffer,
                                                         g_state.app_work_area,
                                                         system_ui_visible,
                                                         terminal_cell_width,
                                                         terminal_cell_height),
                                  true,
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
    g_state.primary_window_session =
        display::make_terminal_window_session(display::kTerminalWindowSessionId,
                                              display::kTerminalAppSurfaceId,
                                              window_session_bounds_for(registered_app->bounds),
                                              terminal_frame_config().visible,
                                              registered_app->visible(),
                                              registered_app->focused,
                                              registered_app->active);
    if (!g_state.window_sessions.register_session(g_state.primary_window_session))
    {
        return false;
    }
    if (!g_state.app_windows.register_window(
            display::app_window_for_session(g_state.primary_window_session)))
    {
        return false;
    }
    if (!g_state.window_stack.register_window(g_state.primary_window_session))
    {
        return false;
    }

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

bool init_dummy_debug_app_layer()
{
    if (!desktop_debug_dummy_app_enabled())
    {
        return true;
    }

    const display::Rect bounds = dummy_app_bounds_for(g_state.app_work_area);
    if (bounds.empty())
    {
        return false;
    }

    g_state.dummy_app_surface =
        display::make_app_surface(display::kDummyDebugAppSurfaceId, bounds, true, false, false);
    if (!g_state.app_surfaces.register_surface(g_state.dummy_app_surface))
    {
        return false;
    }

    const display::AppSurface * registered_app =
        g_state.app_surfaces.find(display::kDummyDebugAppSurfaceId);
    if (registered_app == nullptr ||
        !g_state.targets.register_target(registered_app->display_target()))
    {
        return false;
    }

    g_state.dummy_app_surface = *registered_app;
    g_state.dummy_window_session =
        display::make_app_window_session(display::kDummyDebugWindowSessionId,
                                         display::kDummyDebugAppSurfaceId,
                                         window_session_bounds_for(registered_app->bounds),
                                         display::WindowSessionRole::DummyDebugApp,
                                         terminal_frame_config().visible,
                                         registered_app->visible(),
                                         registered_app->focused,
                                         registered_app->active);
    if (!g_state.window_sessions.register_session(g_state.dummy_window_session))
    {
        return false;
    }
    if (!g_state.app_windows.register_window(
            display::app_window_for_session(g_state.dummy_window_session)))
    {
        return false;
    }
    return g_state.window_stack.register_window(g_state.dummy_window_session);
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

bool sync_primary_window_session_compositor_surface(display::WindowSession session,
                                                    display::Rect previous_bounds)
{
    const display::Rect layer_bounds = session.closed() ? previous_bounds : session.bounds.outer;
    if (!sync_aggregate_app_layer(previous_bounds))
    {
        return false;
    }

    return display::compositor::update_surface(text_caret_surface_for(layer_bounds,
                                                                      !session.closed() &&
                                                                          session.visible() &&
                                                                          session.focused));
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
                             color_for(framebuffer, palette.debug_overlay_background),
                             debug_overlay_config_for_bounds(overlay_bounds)))
    {
        return;
    }
}

display::WindowResizeConstraints terminal_resize_constraints()
{
    return {
        g_state.app_work_area.empty() ? g_state.scene.bounds() : g_state.app_work_area,
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
           g_state.primary_window_session.valid() && !g_state.primary_window_session.closed();
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

    const desktop_bar::Config bar_config = desktop_bar::default_config();
    constexpr display::DisplayPalette palette = display::default_display_palette();
    g_state.terminal_cell_width = terminal_cell_width;
    g_state.terminal_cell_height = terminal_cell_height;
    if (!init_desktop_background_layer(*framebuffer, palette))
    {
        return false;
    }

    const desktop_bar::Layout desktop_layout =
        init_desktop_bar_layer(*framebuffer, palette, bar_config);
    if (!init_primary_app_layer(*framebuffer,
                                palette,
                                desktop_layout.work_area,
                                desktop_layout.bar_visible,
                                terminal_cell_width,
                                terminal_cell_height))
    {
        return false;
    }
    if (!init_dummy_debug_app_layer())
    {
        return false;
    }
    if (!sync_aggregate_app_layer())
    {
        return false;
    }
    sync_desktop_bar_terminal_item();

    init_optional_debug_overlay_layer(*framebuffer,
                                      palette,
                                      debug_overlay_bounds_for_current_layout());

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

bool restore_primary_window_session(display::WindowSession previous,
                                    display::AppSurface previous_app,
                                    display::WindowStack previous_stack)
{
    if (!g_state.window_host.restore_session(previous, previous_app))
    {
        return false;
    }

    g_state.primary_window_session = previous;
    g_state.primary_app_surface = previous.app_surface();
    if (!g_state.app_windows.sync_session(previous))
    {
        return false;
    }
    g_state.window_stack = previous_stack;
    g_state.app_surface_damage.reset(previous.bounds.outer);
    return sync_primary_window_session_compositor_surface(previous, previous.bounds.outer);
}

bool sync_window_session_cache(display::WindowSession session)
{
    if (!g_state.app_windows.sync_session(session))
    {
        return false;
    }

    switch (session.role)
    {
    case display::WindowSessionRole::Terminal:
        g_state.primary_window_session = session;
        g_state.primary_app_surface = session.app_surface();
        g_state.app_surface_damage.reset(session.closed() ? session.bounds.outer
                                                          : session.bounds.outer);
        return true;
    case display::WindowSessionRole::DummyDebugApp:
        g_state.dummy_window_session = session;
        g_state.dummy_app_surface = session.app_surface();
        return true;
    }
    return false;
}

bool sync_window_policy_state_from_stack()
{
    g_state.window_sessions.clear_focus();
    g_state.window_sessions.clear_active();
    g_state.app_surfaces.clear_focus();
    g_state.targets.clear_focus();
    g_state.targets.clear_active();

    for (size_t index = 0; index < g_state.window_stack.size(); ++index)
    {
        const display::WindowStackEntry * entry = g_state.window_stack.at(index);
        const display::WindowSession * current =
            entry == nullptr ? nullptr : g_state.window_host.find(entry->id);
        if (current == nullptr || current->closed())
        {
            continue;
        }

        display::WindowSession next = *current;
        next.state = entry->state;
        next.focused = entry->focused;
        next.active = entry->active;
        if (!g_state.window_host.restore_session(next, next.app_surface()) ||
            !sync_window_session_cache(next))
        {
            return false;
        }
    }

    return true;
}

bool restore_window_manager_result(display::WindowManagerResult result)
{
    if (result.session.previous.role == display::WindowSessionRole::Terminal)
    {
        return restore_primary_window_session(result.session.previous,
                                              result.session.app_surface.previous,
                                              result.previous_stack);
    }

    if (!g_state.window_host.restore_session(result.session.previous,
                                             result.session.app_surface.previous))
    {
        return false;
    }
    g_state.window_stack = result.previous_stack;
    g_state.dummy_window_session = result.session.previous;
    g_state.dummy_app_surface = result.session.app_surface.previous;
    if (!g_state.app_windows.sync_session(result.session.previous))
    {
        return false;
    }
    return sync_aggregate_app_layer(result.session.previous.bounds.outer);
}

bool commit_window_manager_result(display::WindowManagerResult result,
                                  bool resize_app_client)
{
    if (!result.success)
    {
        return false;
    }
    if (!result.changed)
    {
        return true;
    }

    display::WindowSessionMutation mutation = result.session;
    const desktop_bar::TerminalItemState previous_item =
        terminal_item_state_for(g_state.primary_window_session);
    const desktop_bar::WindowItemState previous_dummy_item =
        dummy_item_state_for(g_state.dummy_window_session);
    const display::AppSurface current_app = mutation.current.app_surface();

    if (mutation.current.role == display::WindowSessionRole::Terminal)
    {
        if (!sync_primary_window_session_compositor_surface(mutation.current,
                                                            mutation.previous.bounds.outer))
        {
            restore_window_manager_result(result);
            return false;
        }
    }
    else if (!sync_aggregate_app_layer(mutation.previous.bounds.outer))
    {
        restore_window_manager_result(result);
        return false;
    }

    if (!sync_window_policy_state_from_stack() || !sync_window_session_cache(mutation.current))
    {
        restore_window_manager_result(result);
        return false;
    }

    if (resize_app_client && mutation.current.role == display::WindowSessionRole::Terminal)
    {
        if (g_state.app_resize_callback == nullptr ||
            !g_state.app_resize_callback(current_app))
        {
            restore_window_manager_result(result);
            return false;
        }
    }
    else if (mutation.current.role == display::WindowSessionRole::Terminal &&
             g_state.app_state_callback != nullptr)
    {
        g_state.app_state_callback(current_app);
    }
    const desktop_bar::TerminalItemState current_item =
        terminal_item_state_for(g_state.primary_window_session);
    const desktop_bar::WindowItemState current_dummy_item =
        dummy_item_state_for(g_state.dummy_window_session);
    if (mutation.current.role != display::WindowSessionRole::Terminal &&
        g_state.app_state_callback != nullptr &&
        (previous_item.app_visible != current_item.app_visible ||
         previous_item.app_focused != current_item.app_focused ||
         previous_item.app_active != current_item.app_active ||
         previous_item.app_closed != current_item.app_closed))
    {
        g_state.app_state_callback(g_state.primary_app_surface);
    }
    sync_desktop_bar_terminal_item();
    relayout_debug_overlay_if_present();

    if (!mutation.repaint_bounds.empty())
    {
        display::compositor::repaint_layers_from(display::LayerKind::DesktopBackground,
                                                 mutation.repaint_bounds);
    }
    repaint_changed_window_visual_states(result.previous_stack, result.current_stack);
    if (previous_item.app_visible != current_item.app_visible ||
        previous_item.app_focused != current_item.app_focused ||
        previous_item.app_active != current_item.app_active ||
        previous_item.app_closed != current_item.app_closed ||
        previous_dummy_item.app_visible != current_dummy_item.app_visible ||
        previous_dummy_item.app_focused != current_dummy_item.app_focused ||
        previous_dummy_item.app_active != current_dummy_item.app_active ||
        previous_dummy_item.app_closed != current_dummy_item.app_closed)
    {
        repaint_desktop_bar_if_visible();
    }

    return true;
}

bool resize_app_surface(AppSurfaceId id, Rect bounds)
{
    if (!ready())
    {
        return false;
    }

    const display::WindowSessionId session_id = window_session_id_for_app_surface(id);
    if (session_id == display::kInvalidWindowSessionId ||
        (id == display::kTerminalAppSurfaceId && g_state.app_resize_callback == nullptr))
    {
        return false;
    }

    display::WindowManagerResult result;
    if (!g_state.window_manager.resize_window(session_id,
                                              window_session_bounds_for(bounds),
                                              result))
    {
        return false;
    }

    return commit_window_manager_result(result, id == display::kTerminalAppSurfaceId);
}

bool move_app_surface(AppSurfaceId id, Rect bounds)
{
    if (!ready())
    {
        return false;
    }

    const display::WindowSessionId session_id = window_session_id_for_app_surface(id);
    const display::WindowSession * session = g_state.window_manager.find_window(session_id);
    if (session == nullptr || bounds.width != session->bounds.outer.width ||
        bounds.height != session->bounds.outer.height)
    {
        return false;
    }

    display::WindowManagerResult result;
    if (!g_state.window_manager.move_window(session_id,
                                            window_session_bounds_for(bounds),
                                            result))
    {
        return false;
    }

    return commit_window_manager_result(result, false);
}

bool set_app_surface_visible(AppSurfaceId id, bool visible)
{
    if (!ready())
    {
        return false;
    }

    const display::WindowSessionId session_id = window_session_id_for_app_surface(id);
    if (session_id == display::kInvalidWindowSessionId)
    {
        return false;
    }

    display::WindowManagerResult result;
    const bool mutated =
        visible ? g_state.window_manager.show_window(session_id, result)
                : g_state.window_manager.hide_window(session_id, result);
    if (!mutated)
    {
        return false;
    }

    return commit_window_manager_result(result, false);
}

bool clear_app_surface_focus(AppSurfaceId id)
{
    if (!ready())
    {
        return false;
    }

    const display::WindowSessionId session_id = window_session_id_for_app_surface(id);
    if (session_id == display::kInvalidWindowSessionId)
    {
        return false;
    }

    display::WindowManagerResult result;
    if (!g_state.window_manager.clear_focus(session_id, result))
    {
        return false;
    }

    return commit_window_manager_result(result, false);
}

bool focus_app_surface(AppSurfaceId id)
{
    if (!ready())
    {
        return false;
    }

    const display::WindowSessionId session_id = window_session_id_for_app_surface(id);
    if (session_id == display::kInvalidWindowSessionId)
    {
        return false;
    }

    display::WindowManagerResult result;
    if (!g_state.window_manager.focus_activate_raise_window(session_id, result))
    {
        return false;
    }

    return commit_window_manager_result(result, false);
}

bool close_app_surface(AppSurfaceId id)
{
    if (!ready())
    {
        return false;
    }

    const display::WindowSessionId session_id = window_session_id_for_app_surface(id);
    if (session_id == display::kInvalidWindowSessionId)
    {
        return false;
    }

    display::WindowManagerResult result;
    if (!g_state.window_manager.close_window(session_id, result))
    {
        return false;
    }

    return commit_window_manager_result(result, false);
}

HitTestResult pointer_target()
{
    return g_state.pointer_target;
}

PointerCursorShape pointer_cursor_shape()
{
    return g_state.pointer_cursor_shape;
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
    if (callback == nullptr)
    {
        return false;
    }

    g_state.terminal_app_pixel_callback = callback;
    return display::compositor::register_layer_pixel_callback(display::LayerKind::AppSurface,
                                                              sample_app_layer_pixel);
}

bool register_app_surface_row_source(compositor::LayerRowCallback callback)
{
    if (callback == nullptr)
    {
        return false;
    }

    g_state.terminal_app_row_callback = callback;
    return display::compositor::register_layer_row_callback(display::LayerKind::AppSurface,
                                                            app_layer_row_pixels);
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

display::HitTestResult hit_test_app_windows_by_stack(uint64_t x, uint64_t y)
{
    for (size_t offset = 0; offset < g_state.window_stack.size(); ++offset)
    {
        const size_t index = g_state.window_stack.size() - offset - 1;
        const display::WindowStackEntry * entry = g_state.window_stack.at(index);
        const display::WindowSession * session =
            entry == nullptr ? nullptr : g_state.window_host.find(entry->id);
        if (session == nullptr || !session->visible() || session->closed() ||
            !display::rect_contains(session->bounds.outer, x, y))
        {
            continue;
        }

        const display::SurfaceDescriptor * target =
            g_state.targets.find(display::app_surface_display_id_for(session->app_surface_id));
        if (target == nullptr || !target->valid() ||
            target->kind != display::DisplayTargetKind::AppSurface)
        {
            continue;
        }

        const display::WindowFrameMetrics metrics =
            display::WindowChrome::metrics_for(session->bounds.outer, terminal_frame_config());
        const display::WindowChromeHitRegion chrome_region =
            display::WindowChrome::hit_test(metrics, x, y);
        if (chrome_region == display::WindowChromeHitRegion::None)
        {
            continue;
        }

        return {
            target->id,
            target->kind,
            session->app_surface_id,
            display::kInvalidGuiSurfaceId,
            chrome_region,
            {},
        };
    }

    return {};
}

display::HitTestResult hit_test_runtime_targets(uint64_t x, uint64_t y)
{
    const display::HitTestResult app = hit_test_app_windows_by_stack(x, y);
    if (app.hit())
    {
        return app;
    }

    return display::HitTester(g_state.targets,
                              g_state.app_surfaces,
                              g_state.gui_surfaces,
                              terminal_frame_config())
        .hit_test(x, y);
}

void update_pointer_target(uint64_t x, uint64_t y)
{
    if (!ready())
    {
        g_state.pointer_target = {};
        g_state.pointer_cursor_shape = display::PointerCursorShape::Arrow;
        return;
    }

    g_state.pointer_target = hit_test_runtime_targets(x, y);
    if (g_state.pointer_target.target_kind == display::DisplayTargetKind::GuiSurface &&
        g_state.pointer_target.gui_surface_id == desktop_bar::kGuiSurfaceId)
    {
        g_state.pointer_target.desktop_bar_hit = desktop_bar::hit_test(x, y);
    }
    g_state.pointer_cursor_shape =
        terminal_window_interaction_enabled() &&
                g_state.pointer_target.target_kind == display::DisplayTargetKind::AppSurface
            ? display::cursor_shape_for_hit_region(g_state.pointer_target.app_chrome_region)
            : display::PointerCursorShape::Arrow;
}

bool pointer_targets_desktop_shell_action(desktop_bar::DesktopShellAction action)
{
    return g_state.pointer_target.target_kind == display::DisplayTargetKind::GuiSurface &&
           g_state.pointer_target.gui_surface_id == desktop_bar::kGuiSurfaceId &&
           g_state.pointer_target.desktop_bar_hit.action == action &&
           g_state.pointer_target.desktop_bar_hit.item_enabled;
}

bool focus_window_from_pointer(display::WindowSessionId session_id,
                               TerminalWindowInteractionResult & result)
{
    display::WindowManagerResult manager_result;
    if (!g_state.window_manager.focus_activate_raise_window(session_id, manager_result) ||
        !commit_window_manager_result(manager_result, false))
    {
        return false;
    }

    const display::WindowSession * session = g_state.window_manager.find_window(session_id);
    result.focus_keyboard_terminal_app =
        manager_result.changed && session != nullptr &&
        session->role == display::WindowSessionRole::Terminal;
    result.clear_keyboard_focus =
        manager_result.changed && session != nullptr &&
        session->role != display::WindowSessionRole::Terminal;
    return true;
}

bool dispatch_desktop_shell_action(desktop_bar::DesktopShellAction action,
                                   TerminalWindowInteractionResult & result)
{
    const desktop_shell::WindowCommand command =
        desktop_shell::ActionHandler::command_for(action);
    switch (command)
    {
    case desktop_shell::WindowCommand::None:
        return false;
    case desktop_shell::WindowCommand::TerminalShowFocusRaise:
    {
        display::WindowManagerResult manager_result;
        if (g_state.window_manager.show_focus_activate_raise_window(display::kTerminalWindowSessionId,
                                                                    manager_result) &&
            commit_window_manager_result(manager_result, false))
        {
            result.focus_keyboard_terminal_app = manager_result.changed;
            return manager_result.changed;
        }
        return false;
    }
    case desktop_shell::WindowCommand::DummyDebugAppShowFocusRaise:
    {
        display::WindowManagerResult manager_result;
        if (g_state.window_manager.show_focus_activate_raise_window(
                display::kDummyDebugWindowSessionId,
                manager_result) &&
            commit_window_manager_result(manager_result, false))
        {
            result.focus_keyboard_terminal_app = false;
            result.clear_keyboard_focus = manager_result.changed;
            return manager_result.changed;
        }
        return false;
    }
    }
    return false;
}

bool handle_desktop_bar_pointer(uint64_t x,
                                uint64_t y,
                                bool primary_down,
                                TerminalWindowInteractionResult & result)
{
    if (!desktop_bar_debug_actions_enabled())
    {
        g_state.pressed_desktop_shell_action = desktop_bar::DesktopShellAction::None;
        return false;
    }

    const desktop_bar::DesktopShellAction hovered_action =
        g_state.pointer_target.desktop_bar_hit.action;
    const bool action_target =
        hovered_action != desktop_bar::DesktopShellAction::None &&
        pointer_targets_desktop_shell_action(hovered_action);
    if (primary_down)
    {
        if (action_target)
        {
            g_state.pressed_desktop_shell_action = hovered_action;
            result.handled = true;
            return true;
        }
        if (g_state.pressed_desktop_shell_action != desktop_bar::DesktopShellAction::None)
        {
            result.handled = true;
            return true;
        }
        return false;
    }

    const desktop_bar::DesktopShellAction pressed_action = g_state.pressed_desktop_shell_action;
    if (pressed_action == desktop_bar::DesktopShellAction::None)
    {
        return false;
    }

    g_state.pressed_desktop_shell_action = desktop_bar::DesktopShellAction::None;
    result.handled = true;
    if (action_target && hovered_action == pressed_action &&
        dispatch_desktop_shell_action(pressed_action, result))
    {
        update_pointer_target(x, y);
    }
    return true;
}

TerminalWindowInteractionResult handle_terminal_window_pointer(uint64_t x,
                                                               uint64_t y,
                                                               bool primary_down)
{
    TerminalWindowInteractionResult result;
    if (!ready() || !terminal_window_interaction_enabled())
    {
        g_state.terminal_window_interaction.reset();
        clear_active_window_interaction_target();
        g_state.pressed_desktop_shell_action = desktop_bar::DesktopShellAction::None;
        g_state.pointer_cursor_shape = display::PointerCursorShape::Arrow;
        return result;
    }

    if (!g_state.terminal_window_interaction.active() &&
        handle_desktop_bar_pointer(x, y, primary_down, result))
    {
        return result;
    }

    const bool controller_was_active = g_state.terminal_window_interaction.active();
    const display::WindowSessionId pointer_session_id = pointer_window_session_id();
    const bool pointer_app_target = pointer_session_id != display::kInvalidWindowSessionId;
    const display::WindowSessionId event_session_id =
        controller_was_active && active_window_interaction_target_valid()
            ? g_state.active_window_interaction_session_id
            : pointer_session_id;
    const bool pointer_targets_event_session =
        pointer_app_target && pointer_session_id == event_session_id;
    const display::WindowSession * target_session =
        event_session_id == display::kInvalidWindowSessionId
            ? nullptr
            : g_state.window_manager.find_window(event_session_id);
    const display::WindowChromeHitRegion hit_region =
        pointer_targets_event_session ? g_state.pointer_target.app_chrome_region
                                      : display::WindowChromeHitRegion::None;
    const display::WindowInteractionResult interaction =
        g_state.terminal_window_interaction.update({
            target_session == nullptr ? display::Rect{} : target_session->bounds.outer,
            hit_region,
            x,
            y,
            primary_down,
            terminal_resize_constraints(),
        });
    if (!controller_was_active && interaction.mode != display::WindowInteractionMode::None &&
        pointer_app_target)
    {
        g_state.active_window_interaction_session_id = pointer_session_id;
        g_state.active_window_interaction_app_surface_id = g_state.pointer_target.app_surface_id;
    }
    g_state.pointer_cursor_shape = interaction.cursor_shape;
    result.handled = interaction.handled;

    const display::WindowSessionId interaction_session_id =
        active_window_interaction_target_valid() ? g_state.active_window_interaction_session_id
                                                 : pointer_session_id;
    const display::AppSurfaceId interaction_app_surface_id =
        active_window_interaction_target_valid() ? g_state.active_window_interaction_app_surface_id
                                                 : g_state.pointer_target.app_surface_id;

    if (interaction_session_id != display::kInvalidWindowSessionId &&
        interaction.focus_requested && focus_window_from_pointer(interaction_session_id, result))
    {
        result.handled = true;
        update_pointer_target(x, y);
    }

    if (interaction.mode == display::WindowInteractionMode::Move &&
        !interaction.proposed_bounds.empty())
    {
        const display::WindowSession * session =
            g_state.window_manager.find_window(interaction_session_id);
        const bool moved =
            session != nullptr && !same_rect(session->bounds.outer, interaction.proposed_bounds) &&
            move_app_surface(interaction_app_surface_id, interaction.proposed_bounds);
        if (moved)
        {
            result.app_moved = true;
            update_pointer_target(x, y);
        }
    }
    else if (interaction.commit_resize)
    {
        const bool resized = resize_app_surface(interaction_app_surface_id,
                                                interaction.proposed_bounds);
        if (resized)
        {
            result.app_resized = true;
            update_pointer_target(x, y);
        }
    }
    else if (interaction.close_requested)
    {
        result.clear_keyboard_focus =
            set_app_surface_visible(interaction_app_surface_id, false) &&
            interaction_app_surface_id == display::kTerminalAppSurfaceId;
        update_pointer_target(x, y);
    }

    if (!primary_down && controller_was_active)
    {
        clear_active_window_interaction_target();
    }

    return result;
}

} // namespace kernel::display::runtime
