add_library(os_lab_kernel_console OBJECT
    terminal.cpp
    terminal_app.cpp
    terminal_app_repaint.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_console)
