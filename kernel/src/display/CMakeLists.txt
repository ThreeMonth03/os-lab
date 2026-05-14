add_library(os_lab_kernel_display OBJECT
    display.cpp
    terminal_renderer.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_display)
