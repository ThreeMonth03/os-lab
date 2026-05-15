add_library(os_lab_kernel_text OBJECT
    editor_dirty_range.cpp
    editor_view_layout.cpp
    font5x7.cpp
    history.cpp
    line_editor.cpp
    text_buffer.cpp
    text_console.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_text)
