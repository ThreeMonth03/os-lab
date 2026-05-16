#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/app_surface.hpp"
#include "kernel/display/gui_surface.hpp"

namespace kernel::display
{

struct HitTestResult
{
    SurfaceId surface_id = kInvalidSurfaceId;
    DisplayTargetKind target_kind = DisplayTargetKind::None;
    AppSurfaceId app_surface_id = kInvalidAppSurfaceId;
    GuiSurfaceId gui_surface_id = kInvalidGuiSurfaceId;

    [[nodiscard]] bool hit() const;
};

class HitTester
{
public:
    HitTester(const DisplayTargetRegistry & targets,
              const AppSurfaceRegistry & app_surfaces,
              const GuiSurfaceRegistry & gui_surfaces);

    [[nodiscard]] HitTestResult hit_test(uint64_t x, uint64_t y) const;

private:
    [[nodiscard]] HitTestResult hit_app_surfaces(uint64_t x, uint64_t y) const;
    [[nodiscard]] HitTestResult hit_gui_surfaces(uint64_t x, uint64_t y) const;

    const DisplayTargetRegistry & targets_;
    const AppSurfaceRegistry & app_surfaces_;
    const GuiSurfaceRegistry & gui_surfaces_;
};

[[nodiscard]] bool rect_contains(Rect rect, uint64_t x, uint64_t y);
[[nodiscard]] HitTestResult hit_test(const DisplayTargetRegistry & targets,
                                     const AppSurfaceRegistry & app_surfaces,
                                     const GuiSurfaceRegistry & gui_surfaces,
                                     uint64_t x,
                                     uint64_t y);

} // namespace kernel::display
