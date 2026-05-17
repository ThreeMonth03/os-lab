#include "kernel/display/frame_damage.hpp"

namespace kernel::display
{

void DamageAccumulator::reset(Rect bounds)
{
    bounds_ = bounds;
    clear();
}

void DamageAccumulator::clear()
{
    pending_ = {};
}

void DamageAccumulator::mark_dirty(Rect rect)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty())
    {
        return;
    }

    pending_.dirty_rect = bounding_rect(pending_.dirty_rect, rect);
}

void DamageAccumulator::fallback_scroll_to_dirty(Rect rect)
{
    if (pending_.scroll.valid())
    {
        mark_dirty(pending_.scroll.rect);
        pending_.scroll = {};
    }
    mark_dirty(rect);
}

void DamageAccumulator::record_scroll(Rect rect, uint64_t distance)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty() || distance == 0)
    {
        return;
    }

    if (!pending_.scroll.valid())
    {
        pending_.scroll = {rect, distance};
        mark_dirty(exposed_scroll_region(pending_.scroll));
        return;
    }

    if (pending_.scroll.rect.x == rect.x && pending_.scroll.rect.y == rect.y &&
        pending_.scroll.rect.width == rect.width && pending_.scroll.rect.height == rect.height)
    {
        pending_.scroll.distance += distance;
        if (pending_.scroll.distance >= pending_.scroll.rect.height)
        {
            fallback_scroll_to_dirty(pending_.scroll.rect);
            return;
        }
        mark_dirty(exposed_scroll_region(pending_.scroll));
        return;
    }

    fallback_scroll_to_dirty(rect);
}

FrameDamage DamageAccumulator::flush()
{
    const FrameDamage damage = pending_;
    clear();
    return damage;
}

Rect exposed_scroll_region(ScrollDamage scroll)
{
    if (!scroll.valid())
    {
        return {};
    }

    if (scroll.distance >= scroll.rect.height)
    {
        return scroll.rect;
    }

    return {
        scroll.rect.x,
        scroll.rect.y + scroll.rect.height - scroll.distance,
        scroll.rect.width,
        scroll.distance,
    };
}

} // namespace kernel::display
