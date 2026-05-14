add_library(os_lab_kernel_drivers OBJECT
    debug_port.cpp
    keyboard.cpp
    mouse.cpp
    mouse_cursor.cpp
    ps2_controller.cpp
    serial.cpp
    terminal.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_drivers)
