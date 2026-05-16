#include "kernel/display/hit_test.hpp"

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

} // namespace

namespace kernel::display
{

bool HitTestResult::hit() const
{
    return surface_id != kInvalidSurfaceId && target_kind != DisplayTargetKind::None;
}

HitTester::HitTester(const DisplayTargetRegistry & targets,
                     const AppSurfaceRegistry & app_surfaces,
                     const GuiSurfaceRegistry & gui_surfaces)
    : targets_(targets)
    , app_surfaces_(app_surfaces)
    , gui_surfaces_(gui_surfaces)
{
}

HitTestResult HitTester::hit_test(uint64_t x, uint64_t y) const
{
    const HitTestResult app_surface = hit_app_surfaces(x, y);
    if (app_surface.hit())
    {
        return app_surface;
    }

    const HitTestResult gui_surface = hit_gui_surfaces(x, y);
    if (gui_surface.hit())
    {
        return gui_surface;
    }

    return {};
}

HitTestResult HitTester::hit_app_surfaces(uint64_t x, uint64_t y) const
{
    for (size_t offset = 0; offset < app_surfaces_.size(); ++offset)
    {
        const size_t index = app_surfaces_.size() - offset - 1;
        const AppSurface * surface = app_surfaces_.at(index);
        if (surface == nullptr || !surface->visible || !surface->valid() ||
            !rect_contains(surface->bounds, x, y))
        {
            continue;
        }

        const SurfaceDescriptor * target = targets_.find(surface->display_surface_id);
        if (target == nullptr || !target->valid() ||
            target->kind != DisplayTargetKind::AppSurface)
        {
            continue;
        }

        return {
            target->id,
            target->kind,
            surface->id,
            kInvalidGuiSurfaceId,
        };
    }

    return {};
}

HitTestResult HitTester::hit_gui_surfaces(uint64_t x, uint64_t y) const
{
    for (size_t offset = 0; offset < gui_surfaces_.size(); ++offset)
    {
        const size_t index = gui_surfaces_.size() - offset - 1;
        const GuiSurface * surface = gui_surfaces_.at(index);
        if (surface == nullptr || !surface->visible || !surface->valid() ||
            !rect_contains(surface->bounds, x, y))
        {
            continue;
        }

        const SurfaceDescriptor * target = targets_.find(surface->display_surface_id);
        if (target == nullptr || !target->valid() || target->kind != DisplayTargetKind::GuiSurface)
        {
            continue;
        }

        return {
            target->id,
            target->kind,
            kInvalidAppSurfaceId,
            surface->id,
        };
    }

    return {};
}

bool rect_contains(Rect rect, uint64_t x, uint64_t y)
{
    if (rect.empty())
    {
        return false;
    }

    return x >= rect.x && y >= rect.y && x < saturating_end(rect.x, rect.width) &&
           y < saturating_end(rect.y, rect.height);
}

HitTestResult hit_test(const DisplayTargetRegistry & targets,
                       const AppSurfaceRegistry & app_surfaces,
                       const GuiSurfaceRegistry & gui_surfaces,
                       uint64_t x,
                       uint64_t y)
{
    return HitTester(targets, app_surfaces, gui_surfaces).hit_test(x, y);
}

} // namespace kernel::display
