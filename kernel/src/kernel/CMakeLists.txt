add_library(os_lab_kernel_logic OBJECT
    display.cpp
    history.cpp
    keyboard_decoder.cpp
    line_editor.cpp
    shell_command.cpp
    text_console.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_logic)

add_library(os_lab_kernel_core OBJECT
    main.cpp
    shell.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_core)
