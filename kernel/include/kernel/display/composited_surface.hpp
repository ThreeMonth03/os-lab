#pragma once

#include <stdint.h>

#include "kernel/display/compositor.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"

namespace kernel::display
{

using CompositedSurfaceId = SurfaceId;

enum class CompositedSurfaceRole
{
    None,
    Background,
    SystemUi,
    App,
    Overlay,
    Cursor,
};

enum class SurfaceInputPolicy
{
    None,
    KeyboardFocus,
};

struct CompositedSurfaceDescriptor
{
    CompositedSurfaceId id = kInvalidSurfaceId;
    CompositedSurfaceRole role = CompositedSurfaceRole::None;
    Rect bounds;
    bool visible = true;
    bool active = false;
    bool focused = false;
    LayerOcclusion occlusion = LayerOcclusion::Transparent;
    SurfaceInputPolicy input_policy = SurfaceInputPolicy::None;

    [[nodiscard]] bool valid() const;
    [[nodiscard]] bool accepts_keyboard_focus() const;
    [[nodiscard]] Layer layer() const;
    [[nodiscard]] SurfaceDescriptor display_target() const;
};

[[nodiscard]] LayerKind layer_kind_for(CompositedSurfaceRole role);
[[nodiscard]] DisplayTargetKind display_target_kind_for(CompositedSurfaceRole role);
[[nodiscard]] LayerOcclusion default_occlusion_for(CompositedSurfaceRole role);
[[nodiscard]] SurfaceInputPolicy default_input_policy_for(CompositedSurfaceRole role);
[[nodiscard]] CompositedSurfaceDescriptor make_composited_surface(CompositedSurfaceId id,
                                                                  CompositedSurfaceRole role,
                                                                  Rect bounds,
                                                                  bool visible = true,
                                                                  bool active = false,
                                                                  bool focused = false);

} // namespace kernel::display
