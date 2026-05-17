#include "kernel/display/compositor.hpp"

#include "kernel/display/composited_surface.hpp"

namespace
{

kernel::display::Compositor g_compositor;

struct LayerPixelCallbackSlot
{
    kernel::display::LayerKind kind = kernel::display::LayerKind::None;
    kernel::display::compositor::LayerPixelCallback callback = nullptr;
};

struct LayerBoundsCallbackSlot
{
    kernel::display::LayerKind kind = kernel::display::LayerKind::None;
    kernel::display::compositor::LayerBoundsCallback callback = nullptr;
};

constexpr kernel::display::LayerKind kTopDownLayerOrder[] = {
    kernel::display::LayerKind::MouseCursor,
    kernel::display::LayerKind::TerminalCaret,
    kernel::display::LayerKind::DebugOverlay,
    kernel::display::LayerKind::AppSurface,
    kernel::display::LayerKind::GuiSurface,
    kernel::display::LayerKind::DesktopBackground,
};

LayerPixelCallbackSlot g_layer_pixel_callbacks[kernel::display::kMaxCompositorLayers] = {};
LayerBoundsCallbackSlot g_layer_bounds_callbacks[kernel::display::kMaxCompositorLayers] = {};
kernel::display::SceneBuffer * g_scene_buffer = nullptr;
kernel::display::FramebufferPresenter * g_presenter = nullptr;

kernel::display::compositor::LayerPixelCallback layer_pixel_callback_for(
    kernel::display::LayerKind kind)
{
    for (auto & slot : g_layer_pixel_callbacks)
    {
        if (slot.kind == kind)
        {
            return slot.callback;
        }
    }
    return nullptr;
}

kernel::display::compositor::LayerBoundsCallback layer_bounds_callback_for(
    kernel::display::LayerKind kind)
{
    for (auto & slot : g_layer_bounds_callbacks)
    {
        if (slot.kind == kind)
        {
            return slot.callback;
        }
    }
    return nullptr;
}

bool presenter_overlay_layer(kernel::display::LayerKind kind)
{
    return kind == kernel::display::LayerKind::MouseCursor ||
           kind == kernel::display::LayerKind::TerminalCaret;
}

void refresh_presenter_overlays()
{
    if (g_presenter == nullptr)
    {
        return;
    }

    g_presenter->set_overlay(0,
                             layer_pixel_callback_for(kernel::display::LayerKind::MouseCursor),
                             layer_bounds_callback_for(kernel::display::LayerKind::MouseCursor));
    g_presenter->set_overlay(1,
                             layer_pixel_callback_for(kernel::display::LayerKind::TerminalCaret),
                             layer_bounds_callback_for(kernel::display::LayerKind::TerminalCaret));
}

kernel::display::PixelSample layer_pixel_reader(const kernel::display::LayerPixelSource & source,
                                                uint64_t x,
                                                uint64_t y)
{
    const kernel::display::compositor::LayerPixelCallback callback =
        layer_pixel_callback_for(source.kind);
    if (callback == nullptr)
    {
        return kernel::display::transparent_pixel();
    }
    return callback(x, y);
}

const kernel::display::LayerPixelSource * pixel_source_for(
    kernel::display::LayerKind kind,
    const kernel::display::LayerPixelSource * sources,
    size_t source_count)
{
    for (size_t index = 0; index < source_count; ++index)
    {
        if (sources[index].kind == kind && sources[index].valid() && sources[index].visible)
        {
            return &sources[index];
        }
    }
    return nullptr;
}

size_t collect_pixel_sources(kernel::display::LayerPixelSource (&sources)[kernel::display::kMaxCompositorLayers])
{
    size_t count = 0;
    for (kernel::display::LayerKind kind : kTopDownLayerOrder)
    {
        const kernel::display::Layer * layer = g_compositor.find_layer(kind);
        const kernel::display::compositor::LayerPixelCallback callback = layer_pixel_callback_for(kind);
        if (layer == nullptr || callback == nullptr || presenter_overlay_layer(kind) ||
            count >= kernel::display::kMaxCompositorLayers)
        {
            continue;
        }
        sources[count++] = {
            layer->kind,
            layer->bounds,
            layer->occlusion,
            nullptr,
            layer_pixel_reader,
            layer->visible,
        };
    }
    return count;
}

bool paint_source_region(const kernel::display::LayerPixelSource & source,
                         kernel::display::Rect dirty_rect)
{
    kernel::display::Rect rect = kernel::display::intersect_rect(dirty_rect, source.bounds);
    if (g_scene_buffer == nullptr || !g_scene_buffer->ready() || rect.empty())
    {
        return false;
    }

    bool complete = true;
    for (uint64_t row = 0; row < rect.height; ++row)
    {
        const uint64_t y = rect.y + row;
        for (uint64_t column = 0; column < rect.width; ++column)
        {
            const uint64_t x = rect.x + column;
            const kernel::display::PixelSample sample = source.read(source, x, y);
            if (sample.opaque())
            {
                g_scene_buffer->put_pixel(x, y, sample.color);
                continue;
            }

            if (source.occlusion == kernel::display::LayerOcclusion::Opaque)
            {
                complete = false;
            }
        }
    }
    return complete;
}

bool compose_repaint_plan(const kernel::display::LayerRepaintPlan & plan,
                          const kernel::display::LayerPixelSource * sources,
                          size_t source_count)
{
    if (plan.count == 0)
    {
        return false;
    }

    bool complete = true;
    for (size_t index = 0; index < plan.count; ++index)
    {
        if (presenter_overlay_layer(plan.at(index)))
        {
            continue;
        }

        const kernel::display::LayerPixelSource * source =
            pixel_source_for(plan.at(index), sources, source_count);
        if (source == nullptr)
        {
            return false;
        }
        if (!paint_source_region(*source, plan.rect_at(index)))
        {
            complete = false;
        }
    }
    return complete;
}

bool can_compose_region_from(kernel::display::LayerKind base_layer,
                             kernel::display::Rect dirty_rect,
                             const kernel::display::LayerPixelSource * sources,
                             size_t source_count)
{
    for (uint64_t row = 0; row < dirty_rect.height; ++row)
    {
        const uint64_t y = dirty_rect.y + row;
        for (uint64_t column = 0; column < dirty_rect.width; ++column)
        {
            const uint64_t x = dirty_rect.x + column;
            kernel::display::Color color;
            if (!kernel::display::final_pixel_at(sources, source_count, base_layer, x, y, color))
            {
                return false;
            }
        }
    }
    return true;
}

bool compose_backed_region_from(kernel::display::LayerKind base_layer, kernel::display::Rect dirty_rect)
{
    if (g_scene_buffer == nullptr || !g_scene_buffer->ready() || g_presenter == nullptr ||
        !g_presenter->ready() || dirty_rect.empty())
    {
        return false;
    }

    dirty_rect = kernel::display::intersect_rect(dirty_rect, g_scene_buffer->bounds());
    if (dirty_rect.empty())
    {
        return true;
    }

    kernel::display::LayerPixelSource sources[kernel::display::kMaxCompositorLayers] = {};
    const size_t source_count = collect_pixel_sources(sources);
    const kernel::display::LayerRepaintPlan plan =
        g_compositor.repaint_plan_from(base_layer, dirty_rect);
    if (compose_repaint_plan(plan, sources, source_count))
    {
        return g_presenter->present_rect(dirty_rect);
    }
    if (plan.count == 0 && base_layer == kernel::display::LayerKind::MouseCursor)
    {
        return g_presenter->present_rect(dirty_rect);
    }

    if (!can_compose_region_from(base_layer, dirty_rect, sources, source_count))
    {
        return false;
    }

    for (uint64_t row = 0; row < dirty_rect.height; ++row)
    {
        const uint64_t y = dirty_rect.y + row;
        for (uint64_t column = 0; column < dirty_rect.width; ++column)
        {
            const uint64_t x = dirty_rect.x + column;
            kernel::display::Color color;
            if (kernel::display::final_pixel_at(sources, source_count, base_layer, x, y, color))
            {
                g_scene_buffer->put_pixel(x, y, color);
            }
        }
    }
    return g_presenter->present_rect(dirty_rect);
}

} // namespace

namespace kernel::display::compositor
{

void init(Rect bounds)
{
    g_compositor.reset(bounds);
    g_scene_buffer = nullptr;
    g_presenter = nullptr;
    for (auto & slot : g_layer_pixel_callbacks)
    {
        slot = {};
    }
    for (auto & slot : g_layer_bounds_callbacks)
    {
        slot = {};
    }
}

void set_scene_buffer(SceneBuffer & scene_buffer)
{
    g_scene_buffer = &scene_buffer;
}

void set_presenter(FramebufferPresenter & presenter)
{
    g_presenter = &presenter;
    refresh_presenter_overlays();
}

bool register_surface(CompositedSurfaceDescriptor surface)
{
    return g_compositor.register_surface(surface);
}

bool register_layer_pixel_callback(LayerKind kind, LayerPixelCallback callback)
{
    if (kind == LayerKind::None || callback == nullptr)
    {
        return false;
    }

    for (auto & slot : g_layer_pixel_callbacks)
    {
        if (slot.kind == kind)
        {
            slot.callback = callback;
            if (presenter_overlay_layer(kind))
            {
                refresh_presenter_overlays();
            }
            return true;
        }
    }

    for (auto & slot : g_layer_pixel_callbacks)
    {
        if (slot.kind == LayerKind::None)
        {
            slot = {kind, callback};
            if (presenter_overlay_layer(kind))
            {
                refresh_presenter_overlays();
            }
            return true;
        }
    }

    return false;
}

bool register_layer_bounds_callback(LayerKind kind, LayerBoundsCallback callback)
{
    if (kind == LayerKind::None || callback == nullptr)
    {
        return false;
    }

    for (auto & slot : g_layer_bounds_callbacks)
    {
        if (slot.kind == kind)
        {
            slot.callback = callback;
            if (presenter_overlay_layer(kind))
            {
                refresh_presenter_overlays();
            }
            return true;
        }
    }

    for (auto & slot : g_layer_bounds_callbacks)
    {
        if (slot.kind == LayerKind::None)
        {
            slot = {kind, callback};
            if (presenter_overlay_layer(kind))
            {
                refresh_presenter_overlays();
            }
            return true;
        }
    }

    return false;
}

void repaint_layers_from(LayerKind base_layer, Rect dirty_rect)
{
    compose_backed_region_from(base_layer, dirty_rect);
}

void mark_cursor_move_dirty(Rect old_bounds, Rect new_bounds)
{
    DirtyRectQueue cursor_dirty(g_compositor.bounds());
    kernel::display::mark_cursor_move_dirty(cursor_dirty, old_bounds, new_bounds);

    Rect dirty_rect;
    while (cursor_dirty.pop(dirty_rect))
    {
        repaint_layers_from(LayerKind::DesktopBackground, dirty_rect);
    }
}

} // namespace kernel::display::compositor
