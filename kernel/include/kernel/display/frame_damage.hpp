#pragma once

#include <stdint.h>
#include <stddef.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

struct ScrollDamage
{
    Rect rect;
    uint64_t distance = 0;

    bool valid() const { return !rect.empty() && distance > 0; }
};

enum class FrameDamageStepKind
{
    None,
    DirtyRect,
    Scroll,
};

struct FrameDamageStep
{
    FrameDamageStepKind kind = FrameDamageStepKind::None;
    Rect rect;
    uint64_t distance = 0;
    bool scroll_exposed_dirty = false;

    bool dirty() const { return kind == FrameDamageStepKind::DirtyRect && !rect.empty(); }
    bool scroll() const { return kind == FrameDamageStepKind::Scroll && !rect.empty() && distance > 0; }
};

inline constexpr size_t kMaxFrameDamageSteps = 64;

struct FrameDamage
{
    Rect dirty_rect;
    ScrollDamage scroll;
    FrameDamageStep steps[kMaxFrameDamageSteps] = {};
    size_t step_count = 0;

    bool has_dirty() const { return !dirty_rect.empty(); }
    bool has_scroll() const { return scroll.valid(); }
    bool has_steps() const { return step_count > 0; }
    bool empty() const { return !has_steps() && !has_dirty() && !has_scroll(); }

    [[nodiscard]] bool append_dirty(Rect rect, bool scroll_exposed_dirty = false);
    [[nodiscard]] bool append_scroll(ScrollDamage scroll_damage);
};

class DamageAccumulator
{
public:
    DamageAccumulator() = default;
    explicit DamageAccumulator(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void clear();
    void mark_dirty(Rect rect);
    void record_scroll(Rect rect, uint64_t distance);
    [[nodiscard]] FrameDamage flush();

    Rect bounds() const { return bounds_; }
    FrameDamage pending() const { return pending_; }
    bool empty() const { return pending_.empty(); }

private:
    void fallback_to_final_dirty(Rect rect);

    Rect bounds_;
    FrameDamage pending_;
    bool final_dirty_fallback_ = false;
};

class LayerDamageAccumulator
{
public:
    LayerDamageAccumulator() = default;
    explicit LayerDamageAccumulator(Rect bounds) { reset(bounds); }

    void reset(Rect bounds);
    void clear();
    void record(FrameDamage damage);
    [[nodiscard]] FrameDamage flush();

    Rect bounds() const { return accumulator_.bounds(); }
    bool empty() const { return accumulator_.empty(); }

private:
    void record_dirty(Rect rect);
    void record_scroll(Rect rect, uint64_t distance);

    DamageAccumulator accumulator_;
    Rect scroll_union_;
    uint64_t scroll_step_count_ = 0;
};

[[nodiscard]] Rect exposed_scroll_region(ScrollDamage scroll);

} // namespace kernel::display
