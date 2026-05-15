#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/display.hpp"

namespace kernel::display
{

using SurfaceId = uint16_t;

inline constexpr SurfaceId kInvalidSurfaceId = 0;
inline constexpr SurfaceId kConsoleSurfaceId = 1;
inline constexpr size_t kMaxDisplayTargets = 8;

enum class DisplayTargetKind
{
    None,
    Console,
    DebugOverlay,
    GuiSurface,
};

struct SurfaceDescriptor
{
    SurfaceId id = kInvalidSurfaceId;
    DisplayTargetKind kind = DisplayTargetKind::None;
    Rect bounds;
    bool active = false;
    bool focused = false;

    [[nodiscard]] bool valid() const;
};

class DisplayTargetRegistry
{
public:
    DisplayTargetRegistry() = default;

    void clear();
    [[nodiscard]] bool register_target(SurfaceDescriptor descriptor);
    [[nodiscard]] bool set_active(SurfaceId id);
    [[nodiscard]] bool set_focused(SurfaceId id);

    [[nodiscard]] size_t size() const { return count_; }
    [[nodiscard]] size_t capacity() const { return kMaxDisplayTargets; }
    [[nodiscard]] SurfaceId active_target_id() const { return active_target_id_; }
    [[nodiscard]] SurfaceId focused_target_id() const { return focused_target_id_; }
    [[nodiscard]] const SurfaceDescriptor * find(SurfaceId id) const;
    [[nodiscard]] SurfaceDescriptor active_target() const;

private:
    [[nodiscard]] SurfaceDescriptor * find_mutable(SurfaceId id);
    void sync_target_state();

    SurfaceDescriptor targets_[kMaxDisplayTargets] = {};
    size_t count_ = 0;
    SurfaceId active_target_id_ = kConsoleSurfaceId;
    SurfaceId focused_target_id_ = kConsoleSurfaceId;
};

[[nodiscard]] Rect clip_to_target(const SurfaceDescriptor & target, Rect rect);

} // namespace kernel::display
