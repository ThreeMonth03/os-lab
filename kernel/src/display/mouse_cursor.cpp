#include "kernel/display/mouse_cursor.hpp"

#include "kernel/display/compositor.hpp"
#include "kernel/display/display.hpp"
#include "kernel/boot/limine_support.hpp"
#include "kernel/input/pointer_state.hpp"

namespace
{

namespace display = kernel::display;

constexpr uint64_t kCursorWidth = 10;
constexpr uint64_t kCursorHeight = 16;

constexpr char kCursorBitmap[kCursorHeight][kCursorWidth + 1] = {
    "#.........",
    "##........",
    "#o#.......",
    "#oo#......",
    "#ooo#.....",
    "#oooo#....",
    "#ooooo#...",
    "#oooooo#..",
    "#ooooooo#.",
    "#oooo#....",
    "#oo#o#....",
    "#o#.#o#...",
    "##..#o#...",
    "#....#o#..",
    ".....#o#..",
    "......##..",
};

struct CursorState
{
    display::Surface surface;
    kernel::PointerState pointer;
    uint32_t outline = 0;
    uint32_t fill = 0;
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

uint64_t min_u64(uint64_t lhs, uint64_t rhs) { return lhs < rhs ? lhs : rhs; }

display::Rect current_bounds()
{
    if (!g_state.initialized || !g_state.surface.ready())
    {
        return {};
    }

    return {
        g_state.pointer.x(),
        g_state.pointer.y(),
        min_u64(kCursorWidth, g_state.surface.width() - g_state.pointer.x()),
        min_u64(kCursorHeight, g_state.surface.height() - g_state.pointer.y()),
    };
}

void draw_bitmap()
{
    for (uint64_t row = 0; row < kCursorHeight; ++row)
    {
        for (uint64_t column = 0; column < kCursorWidth; ++column)
        {
            const char pixel = kCursorBitmap[row][column];
            if (pixel == '#')
            {
                g_state.surface.put_pixel(g_state.pointer.x() + column, g_state.pointer.y() + row, {g_state.outline});
            }
            else if (pixel == 'o')
            {
                g_state.surface.put_pixel(g_state.pointer.x() + column, g_state.pointer.y() + row, {g_state.fill});
            }
        }
    }
}

void repaint_cursor(display::Rect dirty_rect)
{
    if (!g_state.initialized || !g_state.visible || !display::rects_overlap(current_bounds(), dirty_rect))
    {
        return;
    }

    draw_bitmap();
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

    g_state.surface = display::Surface(framebuffer->address, framebuffer->width, framebuffer->height, framebuffer->pitch);
    g_state.pointer.reset(framebuffer->width, framebuffer->height, kCursorWidth, kCursorHeight);
    g_state.outline = pack_rgb(*framebuffer, 0x00, 0x00, 0x00);
    g_state.fill = pack_rgb(*framebuffer, 0xff, 0xff, 0xff);
    g_state.initialized = true;
    (void)display::compositor::register_layer({
        display::LayerKind::MouseCursor,
        display::kMouseCursorLayerSurfaceId,
        {0, 0, framebuffer->width, framebuffer->height},
        true,
    });
    (void)display::compositor::register_repaint_callback(display::LayerKind::MouseCursor,
                                                         repaint_cursor);
    return true;
}

bool ready() { return g_state.initialized; }

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

void hide()
{
    if (!g_state.initialized || !g_state.visible)
    {
        return;
    }

    const display::Rect old_bounds = current_bounds();
    g_state.visible = false;
    display::compositor::repaint_layers_from(display::LayerKind::Console, old_bounds);
}

void move_by(int16_t delta_x, int16_t delta_y)
{
    if (!g_state.initialized)
    {
        return;
    }

    const bool was_visible = g_state.visible;
    const display::Rect old_bounds = current_bounds();

    g_state.pointer.move_by(delta_x, delta_y);
    const display::Rect new_bounds = current_bounds();
    if (was_visible && (old_bounds.x != new_bounds.x || old_bounds.y != new_bounds.y))
    {
        display::compositor::mark_cursor_move_dirty(old_bounds, new_bounds);
    }
}

} // namespace kernel::display::mouse_cursor
