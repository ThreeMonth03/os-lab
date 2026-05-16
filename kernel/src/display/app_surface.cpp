#include "kernel/display/app_surface.hpp"

namespace kernel::display
{

bool AppSurface::valid() const
{
    return id != kInvalidAppSurfaceId && display_surface_id != kInvalidSurfaceId &&
           layer_kind != LayerKind::None && !bounds.empty();
}

SurfaceDescriptor AppSurface::display_target() const
{
    return {
        display_surface_id,
        DisplayTargetKind::AppSurface,
        bounds,
        focused,
        focused,
    };
}

Layer AppSurface::layer() const
{
    return {
        layer_kind,
        display_surface_id,
        bounds,
        visible,
        LayerOpacity::Opaque,
    };
}

AppSurface make_app_surface(AppSurfaceId id,
                            Rect bounds,
                            bool visible,
                            bool focused,
                            LayerKind layer_kind)
{
    return {
        id,
        app_surface_display_id_for(id),
        bounds,
        visible,
        focused,
        layer_kind,
    };
}

void AppSurfaceRegistry::clear()
{
    for (size_t index = 0; index < count_; ++index)
    {
        surfaces_[index] = {};
    }
    count_ = 0;
}

bool AppSurfaceRegistry::register_surface(AppSurface surface)
{
    if (!surface.valid() || find(surface.id) != nullptr ||
        find_by_display_surface(surface.display_surface_id) != nullptr || count_ >= kMaxAppSurfaces)
    {
        return false;
    }

    surfaces_[count_++] = surface;
    return true;
}

bool AppSurfaceRegistry::set_focused(AppSurfaceId id)
{
    AppSurface * target = find_mutable(id);
    if (target == nullptr)
    {
        return false;
    }

    clear_focus();
    target->focused = true;
    return true;
}

void AppSurfaceRegistry::clear_focus()
{
    for (size_t index = 0; index < count_; ++index)
    {
        surfaces_[index].focused = false;
    }
}

const AppSurface * AppSurfaceRegistry::find(AppSurfaceId id) const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (surfaces_[index].id == id)
        {
            return &surfaces_[index];
        }
    }

    return nullptr;
}

const AppSurface * AppSurfaceRegistry::find_by_display_surface(SurfaceId id) const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (surfaces_[index].display_surface_id == id)
        {
            return &surfaces_[index];
        }
    }

    return nullptr;
}

const AppSurface * AppSurfaceRegistry::focused_surface() const
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (surfaces_[index].focused)
        {
            return &surfaces_[index];
        }
    }

    return nullptr;
}

const AppSurface * AppSurfaceRegistry::at(size_t index) const
{
    if (index >= count_)
    {
        return nullptr;
    }

    return &surfaces_[index];
}

AppSurface * AppSurfaceRegistry::find_mutable(AppSurfaceId id)
{
    for (size_t index = 0; index < count_; ++index)
    {
        if (surfaces_[index].id == id)
        {
            return &surfaces_[index];
        }
    }

    return nullptr;
}

} // namespace kernel::display
