#include "kernel/display/compositor.hpp"

#include "kernel/display/composited_surface.hpp"

namespace
{

uint64_t saturating_end(uint64_t origin, uint64_t size)
{
    const uint64_t end = origin + size;
    if (end < origin)
    {
        return UINT64_MAX;
    }
    return end;
}

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

bool touches_or_overlaps(kernel::display::Rect lhs, kernel::display::Rect rhs)
{
    const uint64_t lhs_right = saturating_end(lhs.x, lhs.width);
    const uint64_t lhs_bottom = saturating_end(lhs.y, lhs.height);
    const uint64_t rhs_right = saturating_end(rhs.x, rhs.width);
    const uint64_t rhs_bottom = saturating_end(rhs.y, rhs.height);
    return lhs.x <= rhs_right && rhs.x <= lhs_right && lhs.y <= rhs_bottom && rhs.y <= lhs_bottom;
}

kernel::display::Rect clip_to_bounds(kernel::display::Rect rect, kernel::display::Rect bounds)
{
    if (rect.empty() || bounds.empty())
    {
        return {};
    }

    const uint64_t left = rect.x > bounds.x ? rect.x : bounds.x;
    const uint64_t top = rect.y > bounds.y ? rect.y : bounds.y;
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
    const kernel::display::Rect overlap = clip_to_bounds(rect, occluder);
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
                                    kernel::display::Rect occluder)
{
    kernel::display::Rect next_regions[kernel::display::kMaxLayerRepaintEntries] = {};
    size_t next_count = 0;

    for (size_t region_index = 0; region_index < region_count; ++region_index)
    {
        kernel::display::Rect pieces[4] = {};
        const size_t piece_count = subtract_rect(regions[region_index], occluder, pieces);
        for (size_t piece_index = 0; piece_index < piece_count; ++piece_index)
        {
            if (next_count >= kernel::display::kMaxLayerRepaintEntries)
            {
                region_count = 0;
                return false;
            }
            next_regions[next_count++] = pieces[piece_index];
        }
    }

    for (size_t region_index = 0; region_index < next_count; ++region_index)
    {
        regions[region_index] = next_regions[region_index];
    }
    region_count = next_count;
    return true;
}

} // namespace

namespace kernel::display
{

bool Layer::valid() const
{
    return kind != LayerKind::None && !bounds.empty();
}

bool LayerRepaintPlan::push(LayerKind kind, Rect rect)
{
    if (kind == LayerKind::None || rect.empty() || count >= kMaxLayerRepaintEntries)
    {
        return false;
    }

    size_t insert_at = count;
    while (insert_at > 0 && layer_order(kind) < layer_order(entries[insert_at - 1].kind))
    {
        entries[insert_at] = entries[insert_at - 1];
        --insert_at;
    }

    entries[insert_at] = {kind, rect};
    ++count;
    return true;
}

bool LayerRepaintPlan::contains(LayerKind kind) const
{
    for (size_t index = 0; index < count; ++index)
    {
        if (entries[index].kind == kind)
        {
            return true;
        }
    }
    return false;
}

LayerKind LayerRepaintPlan::at(size_t index) const
{
    if (index >= count)
    {
        return LayerKind::None;
    }
    return entries[index].kind;
}

Rect LayerRepaintPlan::rect_at(size_t index) const
{
    if (index >= count)
    {
        return {};
    }
    return entries[index].rect;
}

bool LayerPixelSource::valid() const
{
    return kind != LayerKind::None && !bounds.empty() && read != nullptr;
}

uint8_t layer_order(LayerKind kind)
{
    switch (kind)
    {
    case LayerKind::DesktopBackground:
        return 5;
    case LayerKind::GuiSurface:
        return 8;
    case LayerKind::AppSurface:
        return 10;
    case LayerKind::TerminalCaret:
        return 15;
    case LayerKind::DebugOverlay:
        return 20;
    case LayerKind::MouseCursor:
        return 30;
    case LayerKind::None:
        return 0;
    }
    return 0;
}

bool layer_above(LayerKind candidate, LayerKind reference)
{
    return layer_order(candidate) > layer_order(reference);
}

bool rects_overlap(Rect lhs, Rect rhs)
{
    if (lhs.empty() || rhs.empty())
    {
        return false;
    }

    const uint64_t lhs_right = saturating_end(lhs.x, lhs.width);
    const uint64_t lhs_bottom = saturating_end(lhs.y, lhs.height);
    const uint64_t rhs_right = saturating_end(rhs.x, rhs.width);
    const uint64_t rhs_bottom = saturating_end(rhs.y, rhs.height);
    return lhs.x < rhs_right && rhs.x < lhs_right && lhs.y < rhs_bottom && rhs.y < lhs_bottom;
}

bool should_repaint_layer_after_update(Layer layer, LayerKind updated_layer, Rect dirty_rect)
{
    return layer.visible && layer.valid() && layer_above(layer.kind, updated_layer) &&
           rects_overlap(layer.bounds, dirty_rect);
}

bool final_pixel_at(const LayerPixelSource * sources,
                    size_t source_count,
                    LayerKind base_layer,
                    uint64_t x,
                    uint64_t y,
                    Color & color)
{
    if (sources == nullptr || source_count == 0)
    {
        return false;
    }

    const uint8_t minimum_order = layer_order(base_layer);
    uint8_t next_order = UINT8_MAX;
    while (next_order > minimum_order)
    {
        const LayerPixelSource * top_source = nullptr;
        uint8_t top_order = 0;
        for (size_t index = 0; index < source_count; ++index)
        {
            const LayerPixelSource & source = sources[index];
            if (!source.visible || !source.valid() || !rects_overlap(source.bounds, {x, y, 1, 1}))
            {
                continue;
            }

            const uint8_t order = layer_order(source.kind);
            if (order >= minimum_order && order < next_order && order > top_order)
            {
                top_source = &source;
                top_order = order;
            }
        }

        if (top_source == nullptr)
        {
            return false;
        }

        const PixelSample sample = top_source->read(*top_source, x, y);
        if (sample.opaque())
        {
            color = sample.color;
            return true;
        }

        next_order = top_order;
    }

    return false;
}

void mark_cursor_move_dirty(DirtyRectQueue & queue, Rect old_bounds, Rect new_bounds)
{
    queue.mark_dirty(old_bounds);
    queue.mark_dirty(new_bounds);
}

void DirtyRectQueue::reset(Rect bounds)
{
    bounds_ = bounds;
    clear();
}

void DirtyRectQueue::clear()
{
    for (size_t index = 0; index < count_; ++index)
    {
        rects_[index] = {};
    }
    count_ = 0;
    full_screen_dirty_ = false;
}

DirtyMarkResult DirtyRectQueue::mark_dirty(Rect rect)
{
    rect = clip_to_bounds(rect, bounds_);
    if (bounds_.empty() || rect.empty())
    {
        return DirtyMarkResult::Ignored;
    }

    if (full_screen_dirty_)
    {
        return DirtyMarkResult::Merged;
    }

    for (size_t index = 0; index < count_; ++index)
    {
        if (touches_or_overlaps(rects_[index], rect))
        {
            rects_[index] = clip_to_bounds(bounding_rect(rects_[index], rect), bounds_);
            return DirtyMarkResult::Merged;
        }
    }

    if (count_ >= kMaxDirtyRects)
    {
        return fallback_to_fullscreen();
    }

    rects_[count_++] = rect;
    return DirtyMarkResult::Queued;
}

bool DirtyRectQueue::pop(Rect & rect)
{
    rect = {};
    if (count_ == 0)
    {
        return false;
    }

    rect = rects_[0];
    for (size_t index = 1; index < count_; ++index)
    {
        rects_[index - 1] = rects_[index];
    }
    rects_[--count_] = {};
    if (count_ == 0)
    {
        full_screen_dirty_ = false;
    }
    return true;
}

DirtyMarkResult DirtyRectQueue::fallback_to_fullscreen()
{
    for (size_t index = 0; index < count_; ++index)
    {
        rects_[index] = {};
    }
    count_ = 1;
    rects_[0] = bounds_;
    full_screen_dirty_ = true;
    return DirtyMarkResult::FullscreenFallback;
}

void Compositor::reset(Rect bounds)
{
    bounds_ = bounds;
    clear_layers();
}

void Compositor::clear_layers()
{
    for (size_t index = 0; index < layer_count_; ++index)
    {
        layers_[index] = {};
    }
    layer_count_ = 0;
}

bool Compositor::register_layer(Layer layer)
{
    if (!layer.valid() || find_layer(layer.kind) != nullptr || layer_count_ >= kMaxCompositorLayers)
    {
        return false;
    }

    layers_[layer_count_++] = layer;
    return true;
}

bool Compositor::register_surface(CompositedSurfaceDescriptor surface)
{
    if (!surface.valid())
    {
        return false;
    }
    return register_layer(surface.layer());
}

const Layer * Compositor::find_layer(LayerKind kind) const
{
    for (size_t index = 0; index < layer_count_; ++index)
    {
        if (layers_[index].kind == kind)
        {
            return &layers_[index];
        }
    }

    return nullptr;
}

const Layer * Compositor::top_visible_layer() const
{
    const Layer * top = nullptr;
    for (size_t index = 0; index < layer_count_; ++index)
    {
        if (!layers_[index].visible)
        {
            continue;
        }
        if (top == nullptr || layer_above(layers_[index].kind, top->kind))
        {
            top = &layers_[index];
        }
    }
    return top;
}

LayerRepaintPlan Compositor::repaint_plan_from(LayerKind base_layer, Rect dirty_rect) const
{
    LayerRepaintPlan plan;
    if (dirty_rect.empty())
    {
        return plan;
    }

    const uint8_t minimum_order = layer_order(base_layer);
    for (size_t index = 0; index < layer_count_; ++index)
    {
        const Layer & layer = layers_[index];
        if (!layer.visible || !layer.valid() || layer_order(layer.kind) < minimum_order ||
            !rects_overlap(layer.bounds, dirty_rect))
        {
            continue;
        }

        Rect regions[kMaxLayerRepaintEntries] = {};
        size_t region_count = 1;
        regions[0] = clip_to_bounds(dirty_rect, layer.bounds);

        for (size_t occluder_index = 0; occluder_index < layer_count_; ++occluder_index)
        {
            const Layer & occluder = layers_[occluder_index];
            if (!occluder.visible || !occluder.valid() || !occluder.occludes_lower_repaint() ||
                !layer_above(occluder.kind, layer.kind) || !rects_overlap(occluder.bounds, dirty_rect))
            {
                continue;
            }

            const bool split_ok = subtract_occluder_from_regions(regions, region_count, occluder.bounds);
            if (!split_ok)
            {
                // Capacity fallback: repaint the unsplit layer region rather than dropping damage.
                region_count = 1;
                regions[0] = clip_to_bounds(dirty_rect, layer.bounds);
                break;
            }
            if (region_count == 0)
            {
                break;
            }
        }

        for (size_t region_index = 0; region_index < region_count; ++region_index)
        {
            if (!plan.push(layer.kind, regions[region_index]))
            {
                return plan;
            }
        }
    }

    return plan;
}

LayerRepaintPlan Compositor::repaint_plan_above(LayerKind updated_layer, Rect dirty_rect) const
{
    LayerRepaintPlan plan;
    if (dirty_rect.empty())
    {
        return plan;
    }

    for (size_t index = 0; index < layer_count_; ++index)
    {
        const Layer & layer = layers_[index];
        if (!should_repaint_layer_after_update(layer, updated_layer, dirty_rect))
        {
            continue;
        }
        const Rect repaint_rect = clip_to_bounds(dirty_rect, layer.bounds);
        if (!plan.push(layer.kind, repaint_rect))
        {
            break;
        }
    }

    return plan;
}

} // namespace kernel::display
