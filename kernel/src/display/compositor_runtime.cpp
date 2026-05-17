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
kernel::display::CompositorRuntimeStats g_stats;

uint64_t saturating_end(uint64_t origin, uint64_t size)
{
    const uint64_t end = origin + size;
    return end < origin ? UINT64_MAX : end;
}

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

kernel::display::Rect clip_rect(kernel::display::Rect rect, kernel::display::Rect bounds)
{
    if (rect.empty() || bounds.empty())
    {
        return {};
    }

    const uint64_t left = max_u64(rect.x, bounds.x);
    const uint64_t top = max_u64(rect.y, bounds.y);
    const uint64_t right = min_u64(saturating_end(rect.x, rect.width),
                                   saturating_end(bounds.x, bounds.width));
    const uint64_t bottom = min_u64(saturating_end(rect.y, rect.height),
                                    saturating_end(bounds.y, bounds.height));
    if (right <= left || bottom <= top)
    {
        return {};
    }
    return {left, top, right - left, bottom - top};
}

size_t subtract_rect(kernel::display::Rect rect,
                     kernel::display::Rect occluder,
                     kernel::display::Rect (&pieces)[4])
{
    const kernel::display::Rect overlap = clip_rect(rect, occluder);
    if (overlap.empty())
    {
        pieces[0] = rect;
        return 1;
    }

    const uint64_t rect_right = saturating_end(rect.x, rect.width);
    const uint64_t rect_bottom = saturating_end(rect.y, rect.height);
    const uint64_t overlap_right = saturating_end(overlap.x, overlap.width);
    const uint64_t overlap_bottom = saturating_end(overlap.y, overlap.height);

    size_t count = 0;
    if (overlap.y > rect.y)
    {
        pieces[count++] = {rect.x, rect.y, rect.width, overlap.y - rect.y};
    }
    if (overlap_bottom < rect_bottom)
    {
        pieces[count++] = {rect.x, overlap_bottom, rect.width, rect_bottom - overlap_bottom};
    }
    if (overlap.x > rect.x)
    {
        pieces[count++] = {rect.x, overlap.y, overlap.x - rect.x, overlap.height};
    }
    if (overlap_right < rect_right)
    {
        pieces[count++] = {overlap_right, overlap.y, rect_right - overlap_right, overlap.height};
    }
    return count;
}

bool subtract_occluder_from_regions(kernel::display::Rect (&regions)[kernel::display::kMaxLayerRepaintEntries],
                                    size_t & region_count,
                                    kernel::display::Rect occluder,
                                    kernel::display::Rect & repaint_rect)
{
    kernel::display::Rect next_regions[kernel::display::kMaxLayerRepaintEntries] = {};
    size_t next_count = 0;

    for (size_t region_index = 0; region_index < region_count; ++region_index)
    {
        repaint_rect =
            kernel::display::bounding_rect(repaint_rect, clip_rect(regions[region_index], occluder));

        kernel::display::Rect pieces[4] = {};
        const size_t piece_count = subtract_rect(regions[region_index], occluder, pieces);
        for (size_t piece_index = 0; piece_index < piece_count; ++piece_index)
        {
            if (next_count >= kernel::display::kMaxLayerRepaintEntries)
            {
                return false;
            }
            next_regions[next_count++] = pieces[piece_index];
        }
    }

    for (size_t index = 0; index < next_count; ++index)
    {
        regions[index] = next_regions[index];
    }
    region_count = next_count;
    return true;
}

kernel::display::Rect source_occluder_to_destination(kernel::display::Rect occluder,
                                                     uint64_t distance)
{
    if (occluder.empty())
    {
        return {};
    }

    if (occluder.y >= distance)
    {
        return {occluder.x, occluder.y - distance, occluder.width, occluder.height};
    }

    const uint64_t trimmed = distance - occluder.y;
    if (trimmed >= occluder.height)
    {
        return {};
    }
    return {occluder.x, 0, occluder.width, occluder.height - trimmed};
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
            ++g_stats.scene_compose_pixels;
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
            ++g_stats.scene_preflight_pixels;
            if (!kernel::display::final_pixel_at(sources, source_count, base_layer, x, y, color))
            {
                return false;
            }
        }
    }
    return true;
}

bool compose_backed_region_from(kernel::display::LayerKind base_layer,
                                kernel::display::Rect dirty_rect,
                                bool present)
{
    if (g_scene_buffer == nullptr || !g_scene_buffer->ready() || dirty_rect.empty())
    {
        return false;
    }

    if (present && (g_presenter == nullptr || !g_presenter->ready()))
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
    ++g_stats.repaint_plan_count;
    if (compose_repaint_plan(plan, sources, source_count))
    {
        return !present || g_presenter->present_rect(dirty_rect);
    }
    if (plan.count == 0 && base_layer == kernel::display::LayerKind::MouseCursor)
    {
        return !present || g_presenter->present_rect(dirty_rect);
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
            ++g_stats.scene_compose_pixels;
            if (kernel::display::final_pixel_at(sources, source_count, base_layer, x, y, color))
            {
                g_scene_buffer->put_pixel(x, y, color);
            }
        }
    }
    return !present || g_presenter->present_rect(dirty_rect);
}

kernel::display::Rect copy_scene_scroll_piece(kernel::display::Rect piece, uint64_t distance)
{
    if (g_scene_buffer == nullptr || !g_scene_buffer->ready() || piece.height <= distance)
    {
        return {};
    }

    const kernel::display::Rect source = {
        piece.x,
        piece.y + distance,
        piece.width,
        piece.height - distance,
    };
    const kernel::display::Rect copied = g_scene_buffer->copy_rect(source, piece.x, piece.y);
    g_stats.scene_scroll_copy_pixels += copied.width * copied.height;
    return copied;
}

void append_scene_scroll_regions(kernel::display::LayerKind layer,
                                 kernel::display::Rect rect,
                                 uint64_t distance,
                                 kernel::display::PresentOperationList & operations)
{
    if (g_scene_buffer == nullptr || !g_scene_buffer->ready() || rect.empty() || distance == 0)
    {
        return;
    }

    ++g_stats.scene_scroll_count;
    rect = kernel::display::intersect_rect(rect, g_scene_buffer->bounds());
    if (rect.empty())
    {
        return;
    }

    kernel::display::Rect repaint_rect;
    const kernel::display::LayerRepaintPlan plan = g_compositor.repaint_plan_from(layer, rect);
    for (size_t index = 0; index < plan.count; ++index)
    {
        if (plan.at(index) != layer)
        {
            continue;
        }

        kernel::display::Rect safe_regions[kernel::display::kMaxLayerRepaintEntries] = {};
        size_t safe_count = 1;
        safe_regions[0] = plan.rect_at(index);

        for (kernel::display::LayerKind occluder_kind : kTopDownLayerOrder)
        {
            const kernel::display::Layer * occluder = g_compositor.find_layer(occluder_kind);
            if (occluder == nullptr || !occluder->visible || !occluder->valid() ||
                !occluder->occludes_lower_repaint() ||
                !kernel::display::layer_above(occluder->kind, layer))
            {
                continue;
            }

            const kernel::display::Rect unsafe_source =
                source_occluder_to_destination(occluder->bounds, distance);
            if (unsafe_source.empty())
            {
                continue;
            }

            if (!subtract_occluder_from_regions(safe_regions,
                                                safe_count,
                                                unsafe_source,
                                                repaint_rect))
            {
                repaint_rect = kernel::display::bounding_rect(repaint_rect, plan.rect_at(index));
                safe_count = 0;
                break;
            }
        }

        for (size_t region_index = 0; region_index < safe_count; ++region_index)
        {
            const kernel::display::Rect region = safe_regions[region_index];
            if (region.height <= distance)
            {
                repaint_rect = kernel::display::bounding_rect(repaint_rect, region);
                continue;
            }

            const kernel::display::Rect copied = copy_scene_scroll_piece(region, distance);
            if (!copied.empty())
            {
                operations.append_scroll(region, distance);
            }
        }
    }

    if (!repaint_rect.empty())
    {
        ++g_stats.repaint_plan_fallback_count;
        compose_backed_region_from(layer, repaint_rect, false);
        operations.append_scroll_repair_rect(repaint_rect);
    }
}

} // namespace

namespace kernel::display::compositor
{

void init(Rect bounds)
{
    g_compositor.reset(bounds);
    g_scene_buffer = nullptr;
    g_presenter = nullptr;
    reset_stats();
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
    compose_backed_region_from(base_layer, dirty_rect, true);
}

void append_damage_step_regions(LayerKind base_layer,
                                FrameDamageStep step,
                                PresentOperationList & operations)
{
    if (step.dirty())
    {
        compose_backed_region_from(base_layer, step.rect, false);
        if (step.scroll_exposed_dirty)
        {
            operations.append_scroll_exposed_rect(step.rect);
        }
        else
        {
            operations.append_rect(step.rect);
        }
        return;
    }

    if (step.scroll())
    {
        append_scene_scroll_regions(base_layer, step.rect, step.distance, operations);
        return;
    }
}

PresentOperationList update_scene_from_layer_damage(LayerKind base_layer, FrameDamage damage)
{
    PresentOperationList operations(g_compositor.bounds());
    if (damage.empty())
    {
        return operations;
    }

    if (damage.has_steps())
    {
        for (size_t index = 0; index < damage.step_count; ++index)
        {
            append_damage_step_regions(base_layer, damage.steps[index], operations);
        }
    }
    else
    {
        if (damage.has_scroll())
        {
            append_scene_scroll_regions(base_layer,
                                        damage.scroll.rect,
                                        damage.scroll.distance,
                                        operations);
        }

        if (damage.has_dirty())
        {
            compose_backed_region_from(base_layer, damage.dirty_rect, false);
            operations.append_rect(damage.dirty_rect);
        }
    }

    return operations;
}

void present_scene_operations(const PresentOperationList & operations)
{
    if (g_presenter == nullptr || !g_presenter->ready())
    {
        return;
    }

    for (size_t index = 0; index < operations.count(); ++index)
    {
        const PresentOperation operation = operations.at(index);
        if (operation.rect_present())
        {
            if (!g_presenter->present_rect(operation.rect))
            {
                return;
            }
        }
        else if (operation.scroll_present())
        {
            if (!g_presenter->present_scroll(operation.rect, operation.distance))
            {
                return;
            }
        }
    }
}

void apply_layer_damage(LayerKind base_layer, FrameDamage damage)
{
    present_scene_operations(update_scene_from_layer_damage(base_layer, damage));
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

CompositorRuntimeStats stats()
{
    return g_stats;
}

void reset_stats()
{
    g_stats = {};
}

} // namespace kernel::display::compositor
