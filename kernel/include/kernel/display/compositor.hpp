#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/backing_surface.hpp"
#include "kernel/display/framebuffer_presenter.hpp"
#include "kernel/display/scene_buffer.hpp"

namespace kernel::display
{

struct CompositedSurfaceDescriptor;

inline constexpr size_t kMaxDirtyRects = 16;
inline constexpr size_t kMaxCompositorLayers = 8;
inline constexpr size_t kMaxLayerRepaintEntries = 32;
inline constexpr SurfaceId kMouseCursorLayerSurfaceId = 3;
inline constexpr SurfaceId kTerminalCaretLayerSurfaceId = 4;

class DirtyRectQueue;

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
    DesktopBackground,
    GuiSurface,
    AppSurface,
    TerminalCaret,
    DebugOverlay,
    MouseCursor,
};

enum class LayerOcclusion
{
    Transparent,
    Opaque,
};

struct Layer
{
    LayerKind kind = LayerKind::None;
    SurfaceId surface_id = kInvalidSurfaceId;
    Rect bounds;
    bool visible = true;
    // Repaint occlusion is about compositor planning, not alpha blending.
    // Opaque layers can cover lower-layer repaint regions; transparent layers cannot.
    LayerOcclusion occlusion = LayerOcclusion::Transparent;

    bool valid() const;
    bool occludes_lower_repaint() const { return occlusion == LayerOcclusion::Opaque; }
};

struct LayerRepaintRegion
{
    LayerKind kind = LayerKind::None;
    Rect rect;
};

struct LayerRepaintPlan
{
    LayerRepaintRegion entries[kMaxLayerRepaintEntries] = {};
    size_t count = 0;

    // A single layer can appear more than once when opaque layers split its repaint region.
    bool push(LayerKind kind, Rect rect);
    bool contains(LayerKind kind) const;
    LayerKind at(size_t index) const;
    Rect rect_at(size_t index) const;
};

struct LayerPixelSource;

using LayerPixelReader = PixelSample (*)(const LayerPixelSource & source, uint64_t x, uint64_t y);

struct LayerPixelSource
{
    LayerKind kind = LayerKind::None;
    Rect bounds;
    LayerOcclusion occlusion = LayerOcclusion::Transparent;
    const void * context = nullptr;
    LayerPixelReader read = nullptr;
    bool visible = true;

    [[nodiscard]] bool valid() const;
};

[[nodiscard]] uint8_t layer_order(LayerKind kind);
[[nodiscard]] bool layer_above(LayerKind candidate, LayerKind reference);
[[nodiscard]] bool rects_overlap(Rect lhs, Rect rhs);
[[nodiscard]] bool should_repaint_layer_after_update(Layer layer, LayerKind updated_layer, Rect dirty_rect);
[[nodiscard]] bool final_pixel_at(const LayerPixelSource * sources,
                                  size_t source_count,
                                  LayerKind base_layer,
                                  uint64_t x,
                                  uint64_t y,
                                  Color & color);
void mark_cursor_move_dirty(DirtyRectQueue & queue, Rect old_bounds, Rect new_bounds);

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
    [[nodiscard]] bool register_surface(CompositedSurfaceDescriptor surface);
    [[nodiscard]] const Layer * find_layer(LayerKind kind) const;
    [[nodiscard]] const Layer * top_visible_layer() const;
    [[nodiscard]] LayerRepaintPlan repaint_plan_from(LayerKind base_layer, Rect dirty_rect) const;
    [[nodiscard]] LayerRepaintPlan repaint_plan_above(LayerKind updated_layer, Rect dirty_rect) const;

    size_t layer_count() const { return layer_count_; }
    Rect bounds() const { return bounds_; }

private:
    Rect bounds_;
    Layer layers_[kMaxCompositorLayers] = {};
    size_t layer_count_ = 0;
};

namespace compositor
{

using LayerPixelCallback = PixelSample (*)(uint64_t x, uint64_t y);
using LayerBoundsCallback = Rect (*)();

void init(Rect bounds);
void set_scene_buffer(SceneBuffer & scene_buffer);
void set_presenter(FramebufferPresenter & presenter);
[[nodiscard]] bool register_surface(CompositedSurfaceDescriptor surface);
[[nodiscard]] bool register_layer_pixel_callback(LayerKind kind, LayerPixelCallback callback);
[[nodiscard]] bool register_layer_bounds_callback(LayerKind kind, LayerBoundsCallback callback);
void repaint_layers_from(LayerKind base_layer, Rect dirty_rect);
void scroll_layer_region_up(LayerKind layer, Rect rect, uint64_t distance);
void mark_cursor_move_dirty(Rect old_bounds, Rect new_bounds);

} // namespace compositor

} // namespace kernel::display
