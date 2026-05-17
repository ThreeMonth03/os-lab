#include "kernel/display/composited_surface.hpp"

namespace kernel::display
{

bool CompositedSurfaceDescriptor::valid() const
{
    if (id == kInvalidSurfaceId || role == CompositedSurfaceRole::None || bounds.empty() ||
        layer_kind_for(role) == LayerKind::None)
    {
        return false;
    }

    if ((active || focused) && display_target_kind_for(role) == DisplayTargetKind::None)
    {
        return false;
    }

    return !focused || accepts_keyboard_focus();
}

bool CompositedSurfaceDescriptor::accepts_keyboard_focus() const
{
    return input_policy == SurfaceInputPolicy::KeyboardFocus;
}

Layer CompositedSurfaceDescriptor::layer() const
{
    return {
        layer_kind_for(role),
        id,
        bounds,
        visible,
        occlusion,
    };
}

SurfaceDescriptor CompositedSurfaceDescriptor::display_target() const
{
    const DisplayTargetKind target_kind = display_target_kind_for(role);
    if (target_kind == DisplayTargetKind::None)
    {
        return {};
    }

    return {
        id,
        target_kind,
        bounds,
        active,
        focused,
    };
}

LayerKind layer_kind_for(CompositedSurfaceRole role)
{
    switch (role)
    {
    case CompositedSurfaceRole::Background:
        return LayerKind::DesktopBackground;
    case CompositedSurfaceRole::SystemUi:
        return LayerKind::GuiSurface;
    case CompositedSurfaceRole::App:
        return LayerKind::AppSurface;
    case CompositedSurfaceRole::TextCaret:
        return LayerKind::TerminalCaret;
    case CompositedSurfaceRole::Overlay:
        return LayerKind::DebugOverlay;
    case CompositedSurfaceRole::Cursor:
        return LayerKind::MouseCursor;
    case CompositedSurfaceRole::None:
        return LayerKind::None;
    }
    return LayerKind::None;
}

DisplayTargetKind display_target_kind_for(CompositedSurfaceRole role)
{
    switch (role)
    {
    case CompositedSurfaceRole::SystemUi:
        return DisplayTargetKind::GuiSurface;
    case CompositedSurfaceRole::App:
        return DisplayTargetKind::AppSurface;
    case CompositedSurfaceRole::Overlay:
        return DisplayTargetKind::DebugOverlay;
    case CompositedSurfaceRole::Background:
    case CompositedSurfaceRole::TextCaret:
    case CompositedSurfaceRole::Cursor:
    case CompositedSurfaceRole::None:
        return DisplayTargetKind::None;
    }
    return DisplayTargetKind::None;
}

LayerOcclusion default_occlusion_for(CompositedSurfaceRole role)
{
    switch (role)
    {
    case CompositedSurfaceRole::Background:
    case CompositedSurfaceRole::SystemUi:
    case CompositedSurfaceRole::App:
    case CompositedSurfaceRole::Overlay:
        return LayerOcclusion::Opaque;
    case CompositedSurfaceRole::TextCaret:
    case CompositedSurfaceRole::Cursor:
    case CompositedSurfaceRole::None:
        return LayerOcclusion::Transparent;
    }
    return LayerOcclusion::Transparent;
}

SurfaceInputPolicy default_input_policy_for(CompositedSurfaceRole role)
{
    return role == CompositedSurfaceRole::App ? SurfaceInputPolicy::KeyboardFocus
                                              : SurfaceInputPolicy::None;
}

CompositedSurfaceDescriptor make_composited_surface(CompositedSurfaceId id,
                                                    CompositedSurfaceRole role,
                                                    Rect bounds,
                                                    bool visible,
                                                    bool active,
                                                    bool focused)
{
    return {
        id,
        role,
        bounds,
        visible,
        active,
        focused,
        default_occlusion_for(role),
        default_input_policy_for(role),
    };
}

} // namespace kernel::display
