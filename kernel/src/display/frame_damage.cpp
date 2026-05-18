#include "kernel/display/frame_damage.hpp"

namespace kernel::display
{

namespace
{

bool same_scroll_region(ScrollDamage lhs, ScrollDamage rhs)
{
    return lhs.rect.x == rhs.rect.x && lhs.rect.y == rhs.rect.y &&
           lhs.rect.width == rhs.rect.width && lhs.rect.height == rhs.rect.height;
}

bool can_merge_exposed_scroll(const FrameDamage & damage, ScrollDamage scroll_damage)
{
    if (damage.step_count < 2)
    {
        return false;
    }

    const FrameDamageStep & scroll_step = damage.steps[damage.step_count - 2];
    const FrameDamageStep & dirty_step = damage.steps[damage.step_count - 1];
    return scroll_step.kind == FrameDamageStepKind::Scroll &&
           dirty_step.kind == FrameDamageStepKind::DirtyRect &&
           dirty_step.scroll_exposed_dirty && scroll_step.rect.x == scroll_damage.rect.x &&
           scroll_step.rect.y == scroll_damage.rect.y &&
           scroll_step.rect.width == scroll_damage.rect.width &&
           scroll_step.rect.height == scroll_damage.rect.height;
}

} // namespace

bool FrameDamage::append_dirty(Rect rect, bool scroll_exposed_dirty)
{
    if (rect.empty())
    {
        return true;
    }

    dirty_rect = bounding_rect(dirty_rect, rect);
    if (step_count > 0 && steps[step_count - 1].kind == FrameDamageStepKind::DirtyRect)
    {
        steps[step_count - 1].rect = bounding_rect(steps[step_count - 1].rect, rect);
        steps[step_count - 1].scroll_exposed_dirty =
            steps[step_count - 1].scroll_exposed_dirty && scroll_exposed_dirty;
        return true;
    }

    if (step_count >= kMaxFrameDamageSteps)
    {
        return false;
    }

    steps[step_count] = {FrameDamageStepKind::DirtyRect, rect, 0, scroll_exposed_dirty};
    ++step_count;
    return true;
}

bool FrameDamage::append_scroll(ScrollDamage scroll_damage)
{
    if (!scroll_damage.valid())
    {
        return true;
    }

    if (scroll.valid() && same_scroll_region(scroll, scroll_damage))
    {
        scroll.distance += scroll_damage.distance;
    }
    else if (!scroll.valid())
    {
        scroll = scroll_damage;
    }
    else
    {
        scroll = {};
    }

    if (step_count > 0 && steps[step_count - 1].kind == FrameDamageStepKind::Scroll &&
        steps[step_count - 1].rect.x == scroll_damage.rect.x &&
        steps[step_count - 1].rect.y == scroll_damage.rect.y &&
        steps[step_count - 1].rect.width == scroll_damage.rect.width &&
        steps[step_count - 1].rect.height == scroll_damage.rect.height)
    {
        steps[step_count - 1].distance += scroll_damage.distance;
        return steps[step_count - 1].distance < steps[step_count - 1].rect.height;
    }

    if (step_count >= kMaxFrameDamageSteps)
    {
        return false;
    }

    steps[step_count] = {FrameDamageStepKind::Scroll, scroll_damage.rect, scroll_damage.distance};
    ++step_count;
    return true;
}

void DamageAccumulator::reset(Rect bounds)
{
    bounds_ = bounds;
    clear();
}

void DamageAccumulator::clear()
{
    pending_ = {};
    final_dirty_fallback_ = false;
}

void DamageAccumulator::mark_dirty(Rect rect)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty())
    {
        return;
    }

    if (final_dirty_fallback_)
    {
        pending_.dirty_rect = bounding_rect(pending_.dirty_rect, rect);
        pending_.steps[0] = {FrameDamageStepKind::DirtyRect, pending_.dirty_rect, 0, false};
        pending_.step_count = 1;
        return;
    }

    if (!pending_.append_dirty(rect))
    {
        fallback_to_final_dirty(bounds_);
    }
}

void DamageAccumulator::fallback_to_final_dirty(Rect rect)
{
    pending_ = {};
    final_dirty_fallback_ = true;
    rect = intersect_rect(bounds_, rect);
    if (rect.empty())
    {
        rect = bounds_;
    }
    pending_.dirty_rect = rect;
    pending_.steps[0] = {FrameDamageStepKind::DirtyRect, rect, 0, false};
    pending_.step_count = 1;
}

void DamageAccumulator::record_scroll(Rect rect, uint64_t distance)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty() || distance == 0)
    {
        return;
    }

    if (final_dirty_fallback_)
    {
        mark_dirty(bounds_);
        return;
    }

    if (distance >= rect.height)
    {
        fallback_to_final_dirty(rect);
        return;
    }

    const ScrollDamage scroll{rect, distance};
    if (can_merge_exposed_scroll(pending_, scroll))
    {
        FrameDamageStep & scroll_step = pending_.steps[pending_.step_count - 2];
        FrameDamageStep & exposed_step = pending_.steps[pending_.step_count - 1];
        const uint64_t total_distance = scroll_step.distance + distance;
        if (total_distance >= scroll_step.rect.height)
        {
            fallback_to_final_dirty(scroll_step.rect);
            return;
        }

        scroll_step.distance = total_distance;
        exposed_step.rect = exposed_scroll_region({scroll_step.rect, total_distance});
        pending_.scroll = {scroll_step.rect, total_distance};
        pending_.dirty_rect = bounding_rect(pending_.dirty_rect, exposed_step.rect);
        return;
    }

    if (!pending_.append_scroll(scroll))
    {
        fallback_to_final_dirty(bounds_);
        return;
    }

    const Rect exposed = exposed_scroll_region(scroll);
    if (!pending_.append_dirty(exposed, true))
    {
        fallback_to_final_dirty(bounds_);
    }
}

FrameDamage DamageAccumulator::flush()
{
    const FrameDamage damage = pending_;
    clear();
    return damage;
}

void LayerDamageAccumulator::reset(Rect bounds)
{
    accumulator_.reset(bounds);
    clear();
}

void LayerDamageAccumulator::clear()
{
    accumulator_.clear();
    scroll_union_ = {};
    scroll_step_count_ = 0;
}

void LayerDamageAccumulator::record_dirty(Rect rect)
{
    accumulator_.mark_dirty(rect);
}

void LayerDamageAccumulator::record_scroll(Rect rect, uint64_t distance)
{
    rect = intersect_rect(accumulator_.bounds(), rect);
    if (rect.empty() || distance == 0)
    {
        return;
    }

    accumulator_.record_scroll(rect, distance);
    scroll_union_ = bounding_rect(scroll_union_, rect);
    ++scroll_step_count_;
}

void LayerDamageAccumulator::record(FrameDamage damage)
{
    if (damage.empty())
    {
        return;
    }

    if (damage.has_steps())
    {
        for (size_t index = 0; index < damage.step_count; ++index)
        {
            const FrameDamageStep & step = damage.steps[index];
            if (step.dirty())
            {
                record_dirty(step.rect);
            }
            else if (step.scroll())
            {
                record_scroll(step.rect, step.distance);
            }
        }
        return;
    }

    if (damage.has_scroll())
    {
        record_scroll(damage.scroll.rect, damage.scroll.distance);
    }
    if (damage.has_dirty())
    {
        record_dirty(damage.dirty_rect);
    }
}

FrameDamage LayerDamageAccumulator::flush()
{
    FrameDamage damage = accumulator_.flush();
    const Rect scroll_union = scroll_union_;
    const uint64_t scroll_step_count = scroll_step_count_;
    scroll_union_ = {};
    scroll_step_count_ = 0;

    if (scroll_step_count <= 1 || scroll_union.empty())
    {
        return damage;
    }

    FrameDamage final_damage;
    if (!final_damage.append_dirty(bounding_rect(scroll_union, damage.dirty_rect)))
    {
        return damage;
    }
    return final_damage;
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

Rect scroll_dirty_region_up(Rect dirty_rect, Rect scroll_region, uint64_t distance)
{
    dirty_rect = intersect_rect(dirty_rect, scroll_region);
    if (dirty_rect.empty() || scroll_region.empty() || distance == 0)
    {
        return dirty_rect;
    }

    if (distance >= scroll_region.height)
    {
        return {};
    }

    const Rect retained_region = {
        scroll_region.x,
        scroll_region.y + distance,
        scroll_region.width,
        scroll_region.height - distance,
    };
    const Rect retained_dirty = intersect_rect(dirty_rect, retained_region);
    if (retained_dirty.empty())
    {
        return {};
    }

    return {
        retained_dirty.x,
        retained_dirty.y - distance,
        retained_dirty.width,
        retained_dirty.height,
    };
}

} // namespace kernel::display
