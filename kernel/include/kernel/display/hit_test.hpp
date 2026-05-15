#pragma once

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"
#include "kernel/display/gui_surface.hpp"

namespace kernel::display
{

struct HitTestResult
{
    SurfaceId surface_id = kInvalidSurfaceId;
    DisplayTargetKind target_kind = DisplayTargetKind::None;
    GuiSurfaceId gui_surface_id = kInvalidGuiSurfaceId;

    [[nodiscard]] bool hit() const;
};

class HitTester
{
public:
    HitTester(const DisplayTargetRegistry & targets, const GuiSurfaceRegistry & gui_surfaces);

    [[nodiscard]] HitTestResult hit_test(uint64_t x, uint64_t y) const;

private:
    [[nodiscard]] HitTestResult hit_gui_surfaces(uint64_t x, uint64_t y) const;
    [[nodiscard]] HitTestResult hit_console(uint64_t x, uint64_t y) const;

    const DisplayTargetRegistry & targets_;
    const GuiSurfaceRegistry & gui_surfaces_;
};

[[nodiscard]] bool rect_contains(Rect rect, uint64_t x, uint64_t y);
[[nodiscard]] HitTestResult hit_test(const DisplayTargetRegistry & targets,
                                     const GuiSurfaceRegistry & gui_surfaces,
                                     uint64_t x,
                                     uint64_t y);

} // namespace kernel::display
