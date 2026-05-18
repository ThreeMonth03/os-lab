#include "kernel/display/app_surface.hpp"

namespace kernel::display
{

bool AppSurface::valid() const
{
    return id != kInvalidAppSurfaceId && display_surface_id != kInvalidSurfaceId && !bounds.empty();
}

CompositedSurfaceDescriptor AppSurface::composited_surface() const
{
    if (closed())
    {
        return {};
    }

    const bool surface_visible = visible();
    const bool surface_focused = surface_visible && focused;
    CompositedSurfaceDescriptor surface =
        make_composited_surface(display_surface_id,
                                CompositedSurfaceRole::App,
                                bounds,
                                surface_visible,
                                surface_focused,
                                surface_focused);
    surface.occlusion = LayerOcclusion::Opaque;
    return surface;
}

SurfaceDescriptor AppSurface::display_target() const
{
    return composited_surface().display_target();
}

AppSurface make_app_surface(AppSurfaceId id, Rect bounds, bool visible, bool focused)
{
    return {
        id,
        app_surface_display_id_for(id),
        bounds,
        visible ? AppSurfaceState::Open : AppSurfaceState::Hidden,
        visible && focused,
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
    if (!surface.valid() || surface.closed() || find(surface.id) != nullptr ||
        find_by_display_surface(surface.display_surface_id) != nullptr || count_ >= kMaxAppSurfaces)
    {
        return false;
    }

    surfaces_[count_++] = surface;
    return true;
}

bool AppSurfaceRegistry::set_bounds(AppSurfaceId id, Rect bounds)
{
    if (bounds.empty())
    {
        return false;
    }

    AppSurface * target = find_mutable(id);
    if (target == nullptr || target->closed())
    {
        return false;
    }

    target->bounds = bounds;
    return true;
}

bool AppSurfaceRegistry::set_visible(AppSurfaceId id, bool visible)
{
    AppSurface * target = find_mutable(id);
    if (target == nullptr || target->closed())
    {
        return false;
    }

    target->state = visible ? AppSurfaceState::Open : AppSurfaceState::Hidden;
    if (!visible)
    {
        target->focused = false;
    }
    return true;
}

bool AppSurfaceRegistry::set_focused(AppSurfaceId id)
{
    AppSurface * target = find_mutable(id);
    if (target == nullptr || !target->open())
    {
        return false;
    }

    clear_focus();
    target->focused = true;
    return true;
}

bool AppSurfaceRegistry::close_surface(AppSurfaceId id)
{
    AppSurface * target = find_mutable(id);
    if (target == nullptr || target->closed())
    {
        return false;
    }

    target->state = AppSurfaceState::Closed;
    target->focused = false;
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
        if (surfaces_[index].focused && surfaces_[index].open())
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
