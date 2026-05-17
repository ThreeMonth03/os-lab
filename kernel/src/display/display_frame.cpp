#include "kernel/display/display_frame.hpp"

namespace kernel::display
{

void DisplayFrame::reset(Rect bounds)
{
    depth_ = 0;
    damage_.reset(bounds);
}

void DisplayFrame::begin()
{
    if (depth_ == 0)
    {
        damage_.clear();
    }
    ++depth_;
}

DisplayFrameFlush DisplayFrame::end()
{
    if (depth_ == 0)
    {
        return {};
    }

    --depth_;
    if (depth_ > 0)
    {
        return {};
    }

    return {
        true,
        damage_.flush(),
    };
}

DisplayFrameSubmit DisplayFrame::submit(FrameDamage damage)
{
    if (damage.empty())
    {
        return {};
    }

    if (!in_frame())
    {
        DamageAccumulator immediate(bounds());
        if (damage.has_dirty())
        {
            immediate.mark_dirty(damage.dirty_rect);
        }
        if (damage.has_scroll())
        {
            immediate.record_scroll(damage.scroll.rect, damage.scroll.distance);
        }
        return {
            true,
            immediate.flush(),
        };
    }

    accumulate(damage);
    return {};
}

void DisplayFrame::accumulate(FrameDamage damage)
{
    if (damage.has_steps())
    {
        for (size_t index = 0; index < damage.step_count; ++index)
        {
            const FrameDamageStep step = damage.steps[index];
            if (step.dirty())
            {
                damage_.mark_dirty(step.rect);
            }
            if (step.scroll())
            {
                damage_.record_scroll(step.rect, step.distance);
            }
        }
        return;
    }

    if (damage.has_dirty())
    {
        damage_.mark_dirty(damage.dirty_rect);
    }

    if (damage.has_scroll())
    {
        damage_.record_scroll(damage.scroll.rect, damage.scroll.distance);
    }
}

} // namespace kernel::display
