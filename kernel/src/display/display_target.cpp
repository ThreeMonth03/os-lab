#include "kernel/display/display_target.hpp"

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

uint64_t max_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs > rhs ? lhs : rhs;
}

uint64_t min_u64(uint64_t lhs, uint64_t rhs)
{
    return lhs < rhs ? lhs : rhs;
}

} // namespace

namespace kernel::display
{

bool SurfaceDescriptor::valid() const
{
    return id != kInvalidSurfaceId && kind != DisplayTargetKind::None && !bounds.empty();
}

void DisplayTargetRegistry::clear()
{
    for (size_t index = 0; index < count_; ++index)
    {
        targets_[index] = {};
    }
    count_ = 0;
    active_target_id_ = kInvalidSurfaceId;
    focused_target_id_ = kInvalidSurfaceId;
}

bool DisplayTargetRegistry::register_target(SurfaceDescriptor descriptor)
{
    if (!descriptor.valid() || find(descriptor.id) != nullptr || count_ >= kMaxDisplayTargets)
    {
        return false;
    }

    descriptor.active = descriptor.id == active_target_id_;
    descriptor.focused = descriptor.id == focused_target_id_;
    targets_[count_++] = descriptor;
    return true;
}

bool DisplayTargetRegistry::set_active(SurfaceId id)
{
    if (find(id) == nullptr)
    {
        return false;
    }

    active_target_id_ = id;
    sync_target_state();
    return true;
}

bool DisplayTargetRegistry::set_focused(SurfaceId id)
{
    if (find(id) == nullptr)
    {
        return false;
    }

    focused_target_id_ = id;
    sync_target_state();
    return true;
}

const SurfaceDescriptor * DisplayTargetRegistry::find(SurfaceId id) const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (targets_[index].id == id)
        {
            return &targets_[index];
        }
    }

    return nullptr;
}

SurfaceDescriptor DisplayTargetRegistry::active_target() const
{
    const SurfaceDescriptor * target = find(active_target_id_);
    if (target == nullptr)
    {
        return {};
    }

    return *target;
}

SurfaceDescriptor * DisplayTargetRegistry::find_mutable(SurfaceId id)
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (targets_[index].id == id)
        {
            return &targets_[index];
        }
    }

    return nullptr;
}

void DisplayTargetRegistry::sync_target_state()
{
    for (size_t index = 0; index < count_; ++index)
    {
        targets_[index].active = targets_[index].id == active_target_id_;
        targets_[index].focused = targets_[index].id == focused_target_id_;
    }
}

Rect clip_to_target(const SurfaceDescriptor & target, Rect rect)
{
    if (!target.valid() || rect.empty())
    {
        return {};
    }

    const uint64_t left = max_u64(target.bounds.x, rect.x);
    const uint64_t top = max_u64(target.bounds.y, rect.y);
    const uint64_t right = min_u64(saturating_end(target.bounds.x, target.bounds.width),
                                   saturating_end(rect.x, rect.width));
    const uint64_t bottom = min_u64(saturating_end(target.bounds.y, target.bounds.height),
                                    saturating_end(rect.y, rect.height));

    if (right <= left || bottom <= top)
    {
        return {};
    }

    return {left, top, right - left, bottom - top};
}

} // namespace kernel::display
