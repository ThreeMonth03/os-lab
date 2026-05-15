#include "kernel/display/compositor.hpp"

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

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

kernel::display::Rect union_rect(kernel::display::Rect lhs, kernel::display::Rect rhs)
{
    const uint64_t left = min_u64(lhs.x, rhs.x);
    const uint64_t top = min_u64(lhs.y, rhs.y);
    const uint64_t right = max_u64(saturating_end(lhs.x, lhs.width), saturating_end(rhs.x, rhs.width));
    const uint64_t bottom = max_u64(saturating_end(lhs.y, lhs.height), saturating_end(rhs.y, rhs.height));
    return {left, top, right - left, bottom - top};
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

} // namespace

namespace kernel::display
{

bool Layer::valid() const
{
    return kind != LayerKind::None && !bounds.empty();
}

uint8_t layer_order(LayerKind kind)
{
    switch (kind)
    {
    case LayerKind::Console:
        return 10;
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
            rects_[index] = clip_to_bounds(union_rect(rects_[index], rect), bounds_);
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
    clear_layers();
    dirty_rects_.reset(bounds);
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

} // namespace kernel::display
