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

enum class AppSurfaceState
{
    Closed,
    Hidden,
    Open,
};

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
    AppSurfaceState state = AppSurfaceState::Closed;
    bool focused = false;
    bool active = false;

    bool valid() const;
    bool open() const { return state == AppSurfaceState::Open; }
    bool hidden() const { return state == AppSurfaceState::Hidden; }
    bool closed() const { return state == AppSurfaceState::Closed; }
    bool visible() const { return open(); }
    CompositedSurfaceDescriptor composited_surface() const;
    SurfaceDescriptor display_target() const;
};

AppSurface make_app_surface(AppSurfaceId id,
                            Rect bounds,
                            bool visible = true,
                            bool focused = false,
                            bool active = false);

class AppSurfaceRegistry
{
public:
    AppSurfaceRegistry() = default;

    void clear();
    bool register_surface(AppSurface surface);
    bool restore_surface(AppSurface surface);
    bool set_bounds(AppSurfaceId id, Rect bounds);
    bool set_visible(AppSurfaceId id, bool visible);
    bool set_focused(AppSurfaceId id);
    bool close_surface(AppSurfaceId id);
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
