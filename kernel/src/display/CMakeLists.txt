add_library(os_lab_kernel_display OBJECT
    compositor.cpp
    compositor_runtime.cpp
    debug_overlay.cpp
    debug_overlay_runtime.cpp
    display.cpp
    display_target.cpp
    gui_panel.cpp
    gui_panel_runtime.cpp
    gui_surface.cpp
    hit_test.cpp
    mouse_cursor.cpp
    terminal_render_cache.cpp
    terminal_repaint_state.cpp
    terminal_renderer.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_display)
