#pragma once

#include "kernel/display/app_surface.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"

namespace kernel::display
{

struct AppSurfaceMutation
{
    AppSurface previous;
    AppSurface current;
    Rect repaint_bounds;
};

class AppSurfaceHost
{
public:
    AppSurfaceHost() = default;

    void reset(AppSurfaceRegistry & app_surfaces, DisplayTargetRegistry & targets);

    [[nodiscard]] bool ready() const;
    [[nodiscard]] bool resize_surface(AppSurfaceId id, Rect bounds, AppSurfaceMutation & mutation);
    [[nodiscard]] bool set_visible(AppSurfaceId id, bool visible, AppSurfaceMutation & mutation);
    [[nodiscard]] bool clear_focus(AppSurfaceId id, AppSurfaceMutation & mutation);
    [[nodiscard]] bool focus_surface(AppSurfaceId id, AppSurfaceMutation & mutation);
    [[nodiscard]] bool close_surface(AppSurfaceId id, AppSurfaceMutation & mutation);
    [[nodiscard]] bool restore_surface(AppSurface surface);

private:
    [[nodiscard]] const AppSurface * find(AppSurfaceId id) const;
    [[nodiscard]] bool sync_target(AppSurface surface);
    void clear_target_state_for(AppSurface surface);

    AppSurfaceRegistry * app_surfaces_ = nullptr;
    DisplayTargetRegistry * targets_ = nullptr;
};

} // namespace kernel::display
