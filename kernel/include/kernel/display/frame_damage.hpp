#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

struct ScrollDamage
{
    Rect rect;
    uint64_t distance = 0;

    bool valid() const { return !rect.empty() && distance > 0; }
};

struct FrameDamage
{
    Rect dirty_rect;
    ScrollDamage scroll;

    bool has_dirty() const { return !dirty_rect.empty(); }
    bool has_scroll() const { return scroll.valid(); }
    bool empty() const { return !has_dirty() && !has_scroll(); }
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
    void fallback_scroll_to_dirty(Rect rect);

    Rect bounds_;
    FrameDamage pending_;
};

[[nodiscard]] Rect exposed_scroll_region(ScrollDamage scroll);

} // namespace kernel::display
