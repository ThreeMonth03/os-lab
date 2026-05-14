set(OS_LAB_HOST_LOGIC_DISPLAY_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/display/display.cpp
)

set(OS_LAB_HOST_LOGIC_ARCH_X86_64_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/arch/x86_64/paging.cpp
)

set(OS_LAB_HOST_LOGIC_INPUT_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/input/keyboard_decoder.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/input/mouse_packet_decoder.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/input/pointer_state.cpp
)

set(OS_LAB_HOST_LOGIC_MEMORY_SOURCES
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/heap_allocator.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/memory_map.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/physical_frame_allocator.cpp
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
