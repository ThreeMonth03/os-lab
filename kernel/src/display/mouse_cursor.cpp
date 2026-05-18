#include "kernel/display/mouse_cursor.hpp"

#include "kernel/display/composited_surface.hpp"
#include "kernel/display/compositor.hpp"
#include "kernel/display/cursor_geometry.hpp"
#include "kernel/display/display.hpp"
#include "kernel/boot/limine_support.hpp"
#include "kernel/input/pointer_state.hpp"

namespace
{

namespace display = kernel::display;

constexpr uint64_t kCursorWidth = 16;
constexpr uint64_t kCursorHeight = 16;

constexpr char kCursorBitmap[kCursorHeight][kCursorWidth + 1] = {
    "#...............",
    "##..............",
    "#o#.............",
    "#oo#............",
    "#ooo#...........",
    "#oooo#..........",
    "#ooooo#.........",
    "#oooooo#........",
    "#ooooooo#.......",
    "#oooo#..........",
    "#oo#o#..........",
    "#o#.#o#.........",
    "##..#o#.........",
    "#....#o#........",
    ".....#o#........",
    "......##........",
};

constexpr char kMoveCursorBitmap[kCursorHeight][kCursorWidth + 1] = {
    ".......o........",
    "......ooo.......",
    ".....o.o.o......",
    ".......o........",
    ".......o........",
    "..o....o....o...",
    ".oo....o....oo..",
    "ooooooooooooooo.",
    ".oo....o....oo..",
    "..o....o....o...",
    ".......o........",
    ".......o........",
    ".....o.o.o......",
    "......ooo.......",
    ".......o........",
    "................",
};

constexpr char kResizeHorizontalBitmap[kCursorHeight][kCursorWidth + 1] = {
    "................",
    "................",
    "................",
    "................",
    "................",
    "...o........o...",
    "..oo........oo..",
    ".oooooooooooooo.",
    "..oo........oo..",
    "...o........o...",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
};

constexpr char kResizeVerticalBitmap[kCursorHeight][kCursorWidth + 1] = {
    ".......o........",
    "......ooo.......",
    ".....o.o.o......",
    ".......o........",
    ".......o........",
    ".......o........",
    ".......o........",
    ".......o........",
    ".......o........",
    ".......o........",
    ".......o........",
    ".......o........",
    ".....o.o.o......",
    "......ooo.......",
    ".......o........",
    "................",
};

constexpr char kResizeDiagonalForwardBitmap[kCursorHeight][kCursorWidth + 1] = {
    "..........o.....",
    ".........oo.....",
    "........o.o.....",
    ".......o..o.....",
    "......o...o.....",
    ".....o....o.....",
    "....ooooooooo...",
    ".......o........",
    "......o.........",
    ".....o..........",
    "....o...........",
    "...ooooooooo....",
    ".....o....o.....",
    ".....o...o......",
    ".....o..o.......",
    ".....ooo........",
};

constexpr char kResizeDiagonalBackwardBitmap[kCursorHeight][kCursorWidth + 1] = {
    ".....o..........",
    ".....oo.........",
    ".....o.o........",
    ".....o..o.......",
    ".....o...o......",
    ".....o....o.....",
    "...ooooooooo....",
    "........o.......",
    ".........o......",
    "..........o.....",
    "...........o....",
    "....ooooooooo...",
    ".....o....o.....",
    "......o...o.....",
    ".......o..o.....",
    "........ooo.....",
};

struct CursorState
{
    display::CursorGeometry geometry;
    kernel::input::PointerState pointer;
    uint32_t outline = 0;
    uint32_t fill = 0;
    display::PointerCursorShape shape = display::PointerCursorShape::Arrow;
    bool initialized = false;
    bool visible = false;
};

CursorState g_state;

uint32_t pack_rgb(const limine_framebuffer & framebuffer, uint8_t red, uint8_t green, uint8_t blue)
{
    return (static_cast<uint32_t>(red) << framebuffer.red_mask_shift) |
           (static_cast<uint32_t>(green) << framebuffer.green_mask_shift) |
           (static_cast<uint32_t>(blue) << framebuffer.blue_mask_shift);
}

display::Rect current_bounds()
{
    if (!g_state.initialized)
    {
        return {};
    }

    return g_state.geometry.visible_rect(g_state.pointer.x(), g_state.pointer.y());
}

display::Rect current_damage_bounds()
{
    if (!g_state.initialized)
    {
        return {};
    }

    return g_state.geometry.damage_rect(g_state.pointer.x(), g_state.pointer.y());
}

const char (*cursor_bitmap())[kCursorWidth + 1]
{
    switch (g_state.shape)
    {
    case display::PointerCursorShape::Arrow:
        return kCursorBitmap;
    case display::PointerCursorShape::Move:
        return kMoveCursorBitmap;
    case display::PointerCursorShape::ResizeHorizontal:
        return kResizeHorizontalBitmap;
    case display::PointerCursorShape::ResizeVertical:
        return kResizeVerticalBitmap;
    case display::PointerCursorShape::ResizeDiagonalForward:
        return kResizeDiagonalForwardBitmap;
    case display::PointerCursorShape::ResizeDiagonalBackward:
        return kResizeDiagonalBackwardBitmap;
    }
    return kCursorBitmap;
}

display::PixelSample sample_cursor_pixel(uint64_t x, uint64_t y)
{
    const display::Rect visible = current_bounds();
    if (visible.empty() || x < visible.x || y < visible.y ||
        x >= visible.x + visible.width || y >= visible.y + visible.height)
    {
        return display::transparent_pixel();
    }

    const uint64_t bitmap_row = y - g_state.pointer.y();
    const uint64_t bitmap_column = x - g_state.pointer.x();
    const char pixel = cursor_bitmap()[bitmap_row][bitmap_column];
    if (pixel == '#')
    {
        return display::opaque_pixel({g_state.outline});
    }
    if (pixel == 'o')
    {
        return display::opaque_pixel({g_state.fill});
    }
    return display::transparent_pixel();
}

bool movement_pushed_against_edge(int16_t delta_x, int16_t delta_y)
{
    const int64_t screen_delta_y = -static_cast<int64_t>(delta_y);
    return g_state.geometry.edge_push(g_state.pointer.x(),
                                      g_state.pointer.y(),
                                      static_cast<int64_t>(delta_x),
                                      screen_delta_y);
}

} // namespace

namespace kernel::display::mouse_cursor
{

bool init()
{
    g_state = {};

    const auto * response = boot::framebuffer();
    if (response == nullptr || response->framebuffer_count == 0)
    {
        return false;
    }

    auto * framebuffer = response->framebuffers[0];
    if (framebuffer == nullptr || framebuffer->bpp != 32 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB || framebuffer->width == 0 ||
        framebuffer->height == 0)
    {
        return false;
    }

    g_state.geometry = display::CursorGeometry({0, 0, framebuffer->width, framebuffer->height},
                                               kCursorWidth,
                                               kCursorHeight);
    g_state.pointer.reset(framebuffer->width, framebuffer->height);
    g_state.outline = pack_rgb(*framebuffer, 0x00, 0x00, 0x00);
    g_state.fill = pack_rgb(*framebuffer, 0xff, 0xff, 0xff);
    const display::CompositedSurfaceDescriptor cursor_surface =
        display::make_composited_surface(display::kMouseCursorLayerSurfaceId,
                                         display::CompositedSurfaceRole::Cursor,
                                         {0, 0, framebuffer->width, framebuffer->height});
    const bool layer_registered = display::compositor::register_surface(cursor_surface);
    const bool pixel_source_registered =
        display::compositor::register_layer_pixel_callback(display::LayerKind::MouseCursor,
                                                           sample_cursor_pixel);
    const bool bounds_registered =
        display::compositor::register_layer_bounds_callback(display::LayerKind::MouseCursor,
                                                            current_damage_bounds);
    if (!layer_registered || !pixel_source_registered || !bounds_registered)
    {
        g_state = {};
        return false;
    }

    g_state.initialized = true;
    return true;
}

Position position()
{
    if (!g_state.initialized)
    {
        return {};
    }

    return {g_state.pointer.x(), g_state.pointer.y()};
}

void show()
{
    if (!g_state.initialized || g_state.visible)
    {
        return;
    }

    g_state.visible = true;
    display::compositor::repaint_layers_from(display::LayerKind::MouseCursor, current_bounds());
}

void move_by(int16_t delta_x, int16_t delta_y)
{
    if (!g_state.initialized)
    {
        return;
    }

    const bool was_visible = g_state.visible;
    const display::Rect old_bounds = current_damage_bounds();

    g_state.pointer.move_by(delta_x, delta_y);
    const display::Rect new_bounds = current_damage_bounds();
    if (was_visible && (old_bounds.x != new_bounds.x || old_bounds.y != new_bounds.y))
    {
        display::compositor::mark_cursor_move_dirty(old_bounds, new_bounds);
    }
    else if (was_visible && movement_pushed_against_edge(delta_x, delta_y))
    {
        display::compositor::repaint_layers_from(display::LayerKind::DesktopBackground, new_bounds);
    }
}

void set_shape(PointerCursorShape shape)
{
    if (!g_state.initialized || g_state.shape == shape)
    {
        return;
    }

    const bool was_visible = g_state.visible;
    const display::Rect dirty = current_damage_bounds();
    g_state.shape = shape;
    if (was_visible)
    {
        display::compositor::repaint_layers_from(display::LayerKind::DesktopBackground, dirty);
    }
}

} // namespace kernel::display::mouse_cursor
