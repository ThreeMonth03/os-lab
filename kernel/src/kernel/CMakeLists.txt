add_library(os_lab_kernel_core OBJECT
    main.cpp
    panic.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_core)
