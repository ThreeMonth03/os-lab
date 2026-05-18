#include "kernel/display/app_surface_host.hpp"

namespace kernel::display
{

void AppSurfaceHost::reset(AppSurfaceRegistry & app_surfaces, DisplayTargetRegistry & targets)
{
    app_surfaces_ = &app_surfaces;
    targets_ = &targets;
}

bool AppSurfaceHost::ready() const
{
    return app_surfaces_ != nullptr && targets_ != nullptr;
}

const AppSurface * AppSurfaceHost::find(AppSurfaceId id) const
{
    return ready() ? app_surfaces_->find(id) : nullptr;
}

bool AppSurfaceHost::sync_target(AppSurface surface)
{
    if (!ready() || surface.closed())
    {
        return false;
    }

    return targets_->update_target(surface.display_target());
}

bool AppSurfaceHost::restore_surface(AppSurface previous)
{
    if (!ready() || previous.closed())
    {
        return false;
    }

    if (!app_surfaces_->set_bounds(previous.id, previous.bounds) ||
        !app_surfaces_->set_visible(previous.id, previous.visible()))
    {
        return false;
    }

    if (previous.focused && !app_surfaces_->set_focused(previous.id))
    {
        return false;
    }

    return sync_target(previous);
}

void AppSurfaceHost::clear_target_state_for(AppSurface surface)
{
    if (!ready())
    {
        return;
    }

    if (targets_->active_target_id() == surface.display_surface_id)
    {
        targets_->clear_active();
    }

    if (targets_->focused_target_id() == surface.display_surface_id)
    {
        targets_->clear_focus();
    }
}

bool AppSurfaceHost::resize_surface(AppSurfaceId id, Rect bounds, AppSurfaceMutation & mutation)
{
    mutation = {};
    const AppSurface * current = find(id);
    if (current == nullptr || current->closed() || bounds.empty())
    {
        return false;
    }

    const AppSurface previous = *current;
    if (!app_surfaces_->set_bounds(id, bounds))
    {
        return false;
    }

    const AppSurface * resized = find(id);
    if (resized == nullptr || !sync_target(*resized))
    {
        if (!restore_surface(previous))
        {
            return false;
        }
        return false;
    }

    mutation = {previous, *resized, bounding_rect(previous.bounds, resized->bounds)};
    return true;
}

bool AppSurfaceHost::set_visible(AppSurfaceId id, bool visible, AppSurfaceMutation & mutation)
{
    mutation = {};
    const AppSurface * current = find(id);
    if (current == nullptr || current->closed())
    {
        return false;
    }

    const AppSurface previous = *current;
    if (!app_surfaces_->set_visible(id, visible))
    {
        return false;
    }

    const AppSurface * updated = find(id);
    if (updated == nullptr || !sync_target(*updated))
    {
        if (!restore_surface(previous))
        {
            return false;
        }
        return false;
    }

    mutation = {previous, *updated, previous.bounds};
    return true;
}

bool AppSurfaceHost::focus_surface(AppSurfaceId id, AppSurfaceMutation & mutation)
{
    mutation = {};
    const AppSurface * current = find(id);
    if (current == nullptr || !current->open())
    {
        return false;
    }

    const AppSurface previous = *current;
    if (!app_surfaces_->set_focused(id))
    {
        return false;
    }

    const AppSurface * focused = find(id);
    if (focused == nullptr || !sync_target(*focused))
    {
        if (!restore_surface(previous))
        {
            return false;
        }
        return false;
    }

    mutation = {previous, *focused, {}};
    return true;
}

bool AppSurfaceHost::close_surface(AppSurfaceId id, AppSurfaceMutation & mutation)
{
    mutation = {};
    const AppSurface * current = find(id);
    if (current == nullptr || current->closed())
    {
        return false;
    }

    const AppSurface previous = *current;
    if (!app_surfaces_->close_surface(id))
    {
        return false;
    }

    const AppSurface * closed = find(id);
    if (closed == nullptr)
    {
        if (!restore_surface(previous))
        {
            return false;
        }
        return false;
    }

    clear_target_state_for(previous);
    mutation = {previous, *closed, previous.bounds};
    return true;
}

} // namespace kernel::display
