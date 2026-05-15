#include "kernel/display/compositor.hpp"

namespace
{

kernel::display::Compositor g_compositor;
uint32_t g_redraw_depth = 0;

struct RepaintCallbackSlot
{
    kernel::display::LayerKind kind = kernel::display::LayerKind::None;
    kernel::display::compositor::RepaintCallback callback = nullptr;
};

RepaintCallbackSlot g_callbacks[kernel::display::kMaxCompositorLayers] = {};

kernel::display::compositor::RepaintCallback callback_for(kernel::display::LayerKind kind)
{
    for (auto & slot : g_callbacks)
    {
        if (slot.kind == kind)
        {
            return slot.callback;
        }
    }
    return nullptr;
}

void discard_dirty_queue()
{
    kernel::display::Rect discarded;
    while (g_compositor.pop_dirty(discarded))
    {
    }
}

void repaint_plan(const kernel::display::LayerRepaintPlan & plan, kernel::display::Rect dirty_rect)
{
    for (size_t index = 0; index < plan.count; ++index)
    {
        const kernel::display::compositor::RepaintCallback callback = callback_for(plan.at(index));
        if (callback != nullptr)
        {
            callback(dirty_rect);
        }
    }
}

} // namespace

namespace kernel::display::compositor
{

void init(Rect bounds)
{
    g_compositor.reset(bounds);
    for (auto & slot : g_callbacks)
    {
        slot = {};
    }
    g_redraw_depth = 0;
}

bool register_layer(Layer layer)
{
    return g_compositor.register_layer(layer);
}

bool register_repaint_callback(LayerKind kind, RepaintCallback callback)
{
    if (kind == LayerKind::None || callback == nullptr)
    {
        return false;
    }

    for (auto & slot : g_callbacks)
    {
        if (slot.kind == kind)
        {
            slot.callback = callback;
            return true;
        }
    }

    for (auto & slot : g_callbacks)
    {
        if (slot.kind == LayerKind::None)
        {
            slot = {kind, callback};
            return true;
        }
    }

    return false;
}

void mark_dirty(Rect rect)
{
    g_compositor.mark_dirty(rect);
}

size_t dirty_count()
{
    return g_compositor.dirty_count();
}

bool pop_dirty(Rect & rect)
{
    return g_compositor.pop_dirty(rect);
}

void begin_redraw(Rect dirty_rect)
{
    mark_dirty(dirty_rect);
    ++g_redraw_depth;
}

void end_redraw()
{
    if (g_redraw_depth == 0)
    {
        return;
    }

    --g_redraw_depth;
}

void repaint_layers_above(LayerKind updated_layer, Rect dirty_rect)
{
    repaint_plan(g_compositor.repaint_plan_above(updated_layer, dirty_rect), dirty_rect);
    discard_dirty_queue();
}

void repaint_layers_from(LayerKind base_layer, Rect dirty_rect)
{
    repaint_plan(g_compositor.repaint_plan_from(base_layer, dirty_rect), dirty_rect);
    discard_dirty_queue();
}

void mark_cursor_move_dirty(Rect old_bounds, Rect new_bounds)
{
    DirtyRectQueue cursor_dirty(g_compositor.bounds());
    kernel::display::mark_cursor_move_dirty(cursor_dirty, old_bounds, new_bounds);

    Rect dirty_rect;
    while (cursor_dirty.pop(dirty_rect))
    {
        repaint_layers_from(LayerKind::Console, dirty_rect);
    }
}

void flush_dirty()
{
    Rect dirty_rect;
    while (g_compositor.pop_dirty(dirty_rect))
    {
        repaint_plan(g_compositor.repaint_plan_from(LayerKind::Console, dirty_rect), dirty_rect);
    }
}

RedrawGuard::RedrawGuard(Rect dirty_rect)
{
    begin_redraw(dirty_rect);
}

RedrawGuard::~RedrawGuard()
{
    end_redraw();
}

} // namespace kernel::display::compositor
