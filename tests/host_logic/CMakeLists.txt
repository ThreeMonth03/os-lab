set(OS_LAB_HOST_LOGIC_DISPLAY_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/display/app_surface.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/backing_surface.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/composited_surface.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/compositor.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/cursor_geometry.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/desktop_background.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/debug_overlay.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/debug_overlay_renderer.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/display.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/display_frame.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/display_target.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/framebuffer_presenter.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/frame_damage.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/gui_panel.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/gui_panel_renderer.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/gui_surface.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/hit_test.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/image_view.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/present_region_list.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/scene_buffer.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/terminal_render_cache.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/terminal_repaint_state.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/display/terminal_renderer.cpp
)

set(OS_LAB_HOST_LOGIC_ARCH_X86_64_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/arch/x86_64/paging.cpp
)

set(OS_LAB_HOST_LOGIC_INPUT_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/input/input_router.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/input/keyboard_decoder.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/input/mouse_packet_decoder.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/input/pointer_state.cpp
)

set(OS_LAB_HOST_LOGIC_MEMORY_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/heap_allocator.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/memory_map.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/physical_frame_allocator.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/slab_cache.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/slab_registry.cpp
)

set(OS_LAB_HOST_LOGIC_SHELL_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/shell/shell_command.cpp
)

set(OS_LAB_HOST_LOGIC_TEXT_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/text/editor_dirty_range.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/editor_view_layout.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/font5x7.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/history.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/line_editor.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/text_buffer.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/text_console.cpp
)

add_library(os_lab_host_logic)
target_sources(os_lab_host_logic PRIVATE
    ${OS_LAB_HOST_LOGIC_ARCH_X86_64_SOURCES}
    ${OS_LAB_HOST_LOGIC_DISPLAY_SOURCES}
    ${OS_LAB_HOST_LOGIC_INPUT_SOURCES}
    ${OS_LAB_HOST_LOGIC_MEMORY_SOURCES}
    ${OS_LAB_HOST_LOGIC_SHELL_SOURCES}
    ${OS_LAB_HOST_LOGIC_TEXT_SOURCES}
)
target_link_libraries(os_lab_host_logic PUBLIC
    os_lab_host_compile_settings
)
