#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel/display/compositor.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/display_target.hpp"

namespace kernel::display
{

using GuiSurfaceId = uint16_t;

inline constexpr GuiSurfaceId kInvalidGuiSurfaceId = 0;
inline constexpr size_t kMaxGuiSurfaces = 4;
inline constexpr SurfaceId kFirstGuiSurfaceDisplayId = 100;

[[nodiscard]] constexpr SurfaceId display_surface_id_for(GuiSurfaceId id)
{
    return id == kInvalidGuiSurfaceId ? kInvalidSurfaceId : static_cast<SurfaceId>(kFirstGuiSurfaceDisplayId + id);
}

struct GuiSurface
{
    GuiSurfaceId id = kInvalidGuiSurfaceId;
    SurfaceId display_surface_id = kInvalidSurfaceId;
    Rect bounds;
    bool visible = false;
    bool focusable = false;
    bool focused = false;

    bool valid() const;
    [[nodiscard]] SurfaceDescriptor display_target() const;
    [[nodiscard]] Layer layer() const;
};

[[nodiscard]] GuiSurface make_gui_surface(GuiSurfaceId id, Rect bounds, bool visible, bool focusable);

class GuiSurfaceRegistry
{
public:
    GuiSurfaceRegistry() = default;

    void clear();
    [[nodiscard]] bool register_surface(GuiSurface surface);
    [[nodiscard]] bool set_focused(GuiSurfaceId id);
    void clear_focus();

    [[nodiscard]] const GuiSurface * find(GuiSurfaceId id) const;
    [[nodiscard]] const GuiSurface * find_by_display_surface(SurfaceId id) const;
    [[nodiscard]] const GuiSurface * focused_surface() const;
    [[nodiscard]] const GuiSurface * at(size_t index) const;

    size_t size() const { return count_; }
    size_t capacity() const { return kMaxGuiSurfaces; }

private:
    [[nodiscard]] GuiSurface * find_mutable(GuiSurfaceId id);

    GuiSurface surfaces_[kMaxGuiSurfaces] = {};
    size_t count_ = 0;
};

} // namespace kernel::display
