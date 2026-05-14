add_library(os_lab_kernel_input OBJECT
    input.cpp
    keyboard_decoder.cpp
    mouse_packet_decoder.cpp
    pointer_state.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_input)
