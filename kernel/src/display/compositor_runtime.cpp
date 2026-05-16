#include "kernel/display/compositor.hpp"

#include "kernel/display/composited_surface.hpp"

namespace
{

kernel::display::Compositor g_compositor;

struct LayerRepaintCallbackSlot
{
    kernel::display::LayerKind kind = kernel::display::LayerKind::None;
    kernel::display::compositor::LayerRepaintCallback callback = nullptr;
};

struct LayerPixelCallbackSlot
{
    kernel::display::LayerKind kind = kernel::display::LayerKind::None;
    kernel::display::compositor::LayerPixelCallback callback = nullptr;
};

constexpr kernel::display::LayerKind kTopDownLayerOrder[] = {
    kernel::display::LayerKind::MouseCursor,
    kernel::display::LayerKind::DebugOverlay,
    kernel::display::LayerKind::AppSurface,
    kernel::display::LayerKind::GuiSurface,
    kernel::display::LayerKind::DesktopBackground,
};

LayerRepaintCallbackSlot g_layer_repaint_callbacks[kernel::display::kMaxCompositorLayers] = {};
LayerPixelCallbackSlot g_layer_pixel_callbacks[kernel::display::kMaxCompositorLayers] = {};
kernel::display::Surface * g_front_surface = nullptr;

kernel::display::compositor::LayerRepaintCallback layer_repaint_callback_for(
    kernel::display::LayerKind kind)
{
    for (auto & slot : g_layer_repaint_callbacks)
    {
        if (slot.kind == kind)
        {
            return slot.callback;
        }
    }
    return nullptr;
}

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

size_t collect_pixel_sources(kernel::display::LayerPixelSource (&sources)[kernel::display::kMaxCompositorLayers])
{
    size_t count = 0;
    for (kernel::display::LayerKind kind : kTopDownLayerOrder)
    {
        const kernel::display::Layer * layer = g_compositor.find_layer(kind);
        const kernel::display::compositor::LayerPixelCallback callback = layer_pixel_callback_for(kind);
        if (layer == nullptr || callback == nullptr || count >= kernel::display::kMaxCompositorLayers)
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
    if (g_front_surface == nullptr || !g_front_surface->ready() || dirty_rect.empty())
    {
        return false;
    }

    dirty_rect = kernel::display::intersect_rect(dirty_rect,
                                                 {0, 0, g_front_surface->width(), g_front_surface->height()});
    if (dirty_rect.empty())
    {
        return true;
    }

    kernel::display::LayerPixelSource sources[kernel::display::kMaxCompositorLayers] = {};
    const size_t source_count = collect_pixel_sources(sources);
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
                g_front_surface->put_pixel(x, y, color);
            }
        }
    }
    return true;
}

void repaint_plan(const kernel::display::LayerRepaintPlan & plan)
{
    for (size_t index = 0; index < plan.count; ++index)
    {
        const kernel::display::compositor::LayerRepaintCallback callback =
            layer_repaint_callback_for(plan.at(index));
        if (callback != nullptr)
        {
            callback(plan.rect_at(index));
        }
    }
}

} // namespace

namespace kernel::display::compositor
{

void init(Rect bounds)
{
    g_compositor.reset(bounds);
    g_front_surface = nullptr;
    for (auto & slot : g_layer_repaint_callbacks)
    {
        slot = {};
    }
    for (auto & slot : g_layer_pixel_callbacks)
    {
        slot = {};
    }
}

void set_framebuffer_surface(Surface & surface)
{
    g_front_surface = &surface;
}

bool register_surface(CompositedSurfaceDescriptor surface)
{
    return g_compositor.register_surface(surface);
}

bool register_layer_repaint_callback(LayerKind kind, LayerRepaintCallback callback)
{
    if (kind == LayerKind::None || callback == nullptr)
    {
        return false;
    }

    for (auto & slot : g_layer_repaint_callbacks)
    {
        if (slot.kind == kind)
        {
            slot.callback = callback;
            return true;
        }
    }

    for (auto & slot : g_layer_repaint_callbacks)
    {
        if (slot.kind == LayerKind::None)
        {
            slot = {kind, callback};
            return true;
        }
    }

    return false;
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
            return true;
        }
    }

    for (auto & slot : g_layer_pixel_callbacks)
    {
        if (slot.kind == LayerKind::None)
        {
            slot = {kind, callback};
            return true;
        }
    }

    return false;
}

void repaint_layers_above(LayerKind updated_layer, Rect dirty_rect)
{
    repaint_plan(g_compositor.repaint_plan_above(updated_layer, dirty_rect));
}

void repaint_layers_from(LayerKind base_layer, Rect dirty_rect)
{
    if (compose_backed_region_from(base_layer, dirty_rect))
    {
        return;
    }
    repaint_plan(g_compositor.repaint_plan_from(base_layer, dirty_rect));
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
