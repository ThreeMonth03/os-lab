find_package(GTest REQUIRED)

add_library(os_lab_host_logic
    ${PROJECT_SOURCE_DIR}/kernel/src/display/display.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/input/keyboard_decoder.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/input/mouse_packet_decoder.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/input/pointer_state.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/memory_map.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/memory/physical_frame_allocator.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/shell/shell_command.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/editor_dirty_range.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/editor_view_layout.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/font5x7.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/history.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/line_editor.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/text/text_console.cpp
)
target_compile_features(os_lab_host_logic PUBLIC cxx_std_23)
target_include_directories(os_lab_host_logic PUBLIC
    ${PROJECT_SOURCE_DIR}/kernel/include
)
target_compile_options(os_lab_host_logic PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_subdirectory(unit)
