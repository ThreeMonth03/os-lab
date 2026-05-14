add_library(os_lab_kernel_logic OBJECT
    display.cpp
    editor_dirty_range.cpp
    editor_view_layout.cpp
    font5x7.cpp
    history.cpp
    input.cpp
    keyboard_decoder.cpp
    line_editor.cpp
    memory_map.cpp
    mouse_packet_decoder.cpp
    physical_frame_allocator.cpp
    pointer_state.cpp
    shell_command.cpp
    text_console.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_logic)

add_library(os_lab_kernel_core OBJECT
    main.cpp
    panic.cpp
    shell.cpp
    timer_smoke.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_core)
