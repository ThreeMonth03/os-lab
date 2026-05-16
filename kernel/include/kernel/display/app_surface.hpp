#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/composited_surface.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"

namespace kernel::display
{

using AppSurfaceId = uint16_t;

inline constexpr AppSurfaceId kInvalidAppSurfaceId = 0;
inline constexpr AppSurfaceId kTerminalAppSurfaceId = 1;
inline constexpr size_t kMaxAppSurfaces = 4;
inline constexpr SurfaceId kFirstAppSurfaceDisplayId = 200;

constexpr SurfaceId app_surface_display_id_for(AppSurfaceId id)
{
    return id == kInvalidAppSurfaceId ? kInvalidSurfaceId
                                      : static_cast<SurfaceId>(kFirstAppSurfaceDisplayId + id);
}

struct AppSurface
{
    AppSurfaceId id = kInvalidAppSurfaceId;
    SurfaceId display_surface_id = kInvalidSurfaceId;
    Rect bounds;
    bool visible = false;
    bool focused = false;

    bool valid() const;
    CompositedSurfaceDescriptor composited_surface() const;
    SurfaceDescriptor display_target() const;
};

AppSurface make_app_surface(AppSurfaceId id,
                            Rect bounds,
                            bool visible = true,
                            bool focused = false);

class AppSurfaceRegistry
{
public:
    AppSurfaceRegistry() = default;

    void clear();
    bool register_surface(AppSurface surface);
    bool set_focused(AppSurfaceId id);
    void clear_focus();

    const AppSurface * find(AppSurfaceId id) const;
    const AppSurface * find_by_display_surface(SurfaceId id) const;
    const AppSurface * focused_surface() const;
    const AppSurface * at(size_t index) const;

    size_t size() const { return count_; }
    size_t capacity() const { return kMaxAppSurfaces; }

private:
    AppSurface * find_mutable(AppSurfaceId id);

    AppSurface surfaces_[kMaxAppSurfaces] = {};
    size_t count_ = 0;
};

} // namespace kernel::display
