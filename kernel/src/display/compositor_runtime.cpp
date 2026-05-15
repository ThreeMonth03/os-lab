#include "kernel/display/compositor.hpp"

#include "kernel/display/mouse_cursor.hpp"

namespace
{

kernel::display::Compositor g_compositor;

} // namespace

namespace kernel::display::compositor
{

void init(Rect bounds)
{
    g_compositor.reset(bounds);
}

bool register_layer(Layer layer)
{
    return g_compositor.register_layer(layer);
}

void mark_dirty(Rect rect)
{
    (void)g_compositor.mark_dirty(rect);
}

size_t dirty_count()
{
    return g_compositor.dirty_count();
}

bool pop_dirty(Rect & rect)
{
    return g_compositor.pop_dirty(rect);
}

RedrawGuard::RedrawGuard(Rect dirty_rect)
{
    mark_dirty(dirty_rect);
    mouse_cursor::hide();
}

RedrawGuard::~RedrawGuard()
{
    mouse_cursor::show();
}

} // namespace kernel::display::compositor
