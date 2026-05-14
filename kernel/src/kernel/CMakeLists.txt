add_library(os_lab_kernel_logic OBJECT
    history.cpp
    line_editor.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_logic)

add_library(os_lab_kernel_core OBJECT
    main.cpp
    shell.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_core)
