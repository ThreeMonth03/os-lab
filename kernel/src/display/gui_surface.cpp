#include "kernel/display/gui_surface.hpp"

namespace kernel::display
{

bool GuiSurface::valid() const
{
    return id != kInvalidGuiSurfaceId && display_surface_id != kInvalidSurfaceId && !bounds.empty() &&
           (!focused || focusable);
}

SurfaceDescriptor GuiSurface::display_target() const
{
    return {
        display_surface_id,
        DisplayTargetKind::GuiSurface,
        bounds,
        false,
        focused,
    };
}

Layer GuiSurface::layer() const
{
    return {
        LayerKind::GuiSurface,
        display_surface_id,
        bounds,
        visible,
    };
}

GuiSurface make_gui_surface(GuiSurfaceId id, Rect bounds, bool visible, bool focusable)
{
    return {
        id,
        display_surface_id_for(id),
        bounds,
        visible,
        focusable,
        false,
    };
}

void GuiSurfaceRegistry::clear()
{
    for (size_t index = 0; index < count_; ++index)
    {
        surfaces_[index] = {};
    }
    count_ = 0;
}

bool GuiSurfaceRegistry::register_surface(GuiSurface surface)
{
    if (!surface.valid() || find(surface.id) != nullptr ||
        find_by_display_surface(surface.display_surface_id) != nullptr || count_ >= kMaxGuiSurfaces)
    {
        return false;
    }

    surfaces_[count_++] = surface;
    return true;
}

bool GuiSurfaceRegistry::set_focused(GuiSurfaceId id)
{
    GuiSurface * target = find_mutable(id);
    if (target == nullptr || !target->focusable)
    {
        return false;
    }

    clear_focus();
    target->focused = true;
    return true;
}

void GuiSurfaceRegistry::clear_focus()
{
    for (size_t index = 0; index < count_; ++index)
    {
        surfaces_[index].focused = false;
    }
}

const GuiSurface * GuiSurfaceRegistry::find(GuiSurfaceId id) const
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

const GuiSurface * GuiSurfaceRegistry::find_by_display_surface(SurfaceId id) const
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

const GuiSurface * GuiSurfaceRegistry::focused_surface() const
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

const GuiSurface * GuiSurfaceRegistry::at(size_t index) const
{
    if (index >= count_)
    {
        return nullptr;
    }

    return &surfaces_[index];
}

GuiSurface * GuiSurfaceRegistry::find_mutable(GuiSurfaceId id)
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
