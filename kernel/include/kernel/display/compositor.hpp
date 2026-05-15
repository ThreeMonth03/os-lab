#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"

namespace kernel::display
{

inline constexpr size_t kMaxDirtyRects = 16;
inline constexpr size_t kMaxCompositorLayers = 8;
inline constexpr SurfaceId kMouseCursorLayerSurfaceId = 3;

enum class DirtyMarkResult
{
    Ignored,
    Queued,
    Merged,
    FullscreenFallback,
};

enum class LayerKind
{
    None,
    Console,
    GuiSurface,
    DebugOverlay,
    MouseCursor,
};

struct Layer
{
    LayerKind kind = LayerKind::None;
    SurfaceId surface_id = kInvalidSurfaceId;
    Rect bounds;
    bool visible = true;

    bool valid() const;
};

[[nodiscard]] uint8_t layer_order(LayerKind kind);
[[nodiscard]] bool layer_above(LayerKind candidate, LayerKind reference);
[[nodiscard]] bool rects_overlap(Rect lhs, Rect rhs);
[[nodiscard]] bool should_repaint_layer_after_update(Layer layer, LayerKind updated_layer, Rect dirty_rect);

class DirtyRectQueue
{
public:
    DirtyRectQueue() = default;
    explicit DirtyRectQueue(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void clear();
    DirtyMarkResult mark_dirty(Rect rect);
    [[nodiscard]] bool pop(Rect & rect);

    bool empty() const { return count_ == 0; }
    size_t size() const { return count_; }
    size_t capacity() const { return kMaxDirtyRects; }
    bool full_screen_dirty() const { return full_screen_dirty_; }
    Rect bounds() const { return bounds_; }

private:
    DirtyMarkResult fallback_to_fullscreen();

    Rect bounds_;
    Rect rects_[kMaxDirtyRects] = {};
    size_t count_ = 0;
    bool full_screen_dirty_ = false;
};

class Compositor
{
public:
    Compositor() = default;
    explicit Compositor(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void clear_layers();
    [[nodiscard]] bool register_layer(Layer layer);
    [[nodiscard]] const Layer * find_layer(LayerKind kind) const;
    [[nodiscard]] const Layer * top_visible_layer() const;

    DirtyMarkResult mark_dirty(Rect rect) { return dirty_rects_.mark_dirty(rect); }
    [[nodiscard]] bool pop_dirty(Rect & rect) { return dirty_rects_.pop(rect); }

    size_t layer_count() const { return layer_count_; }
    size_t layer_capacity() const { return kMaxCompositorLayers; }
    size_t dirty_count() const { return dirty_rects_.size(); }
    bool full_screen_dirty() const { return dirty_rects_.full_screen_dirty(); }
    Rect bounds() const { return dirty_rects_.bounds(); }

private:
    Layer layers_[kMaxCompositorLayers] = {};
    size_t layer_count_ = 0;
    DirtyRectQueue dirty_rects_;
};

namespace compositor
{

void init(Rect bounds);
[[nodiscard]] bool register_layer(Layer layer);
void mark_dirty(Rect rect);
size_t dirty_count();
[[nodiscard]] bool pop_dirty(Rect & rect);
void begin_redraw(Rect dirty_rect = {});
void end_redraw();
void repaint_layers_above(LayerKind updated_layer, Rect dirty_rect);

class RedrawGuard
{
public:
    explicit RedrawGuard(Rect dirty_rect = {});
    ~RedrawGuard();

    RedrawGuard(const RedrawGuard &) = delete;
    RedrawGuard & operator=(const RedrawGuard &) = delete;
    RedrawGuard(RedrawGuard &&) = delete;
    RedrawGuard & operator=(RedrawGuard &&) = delete;
};

} // namespace compositor

} // namespace kernel::display
