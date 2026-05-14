add_library(os_lab_kernel_boot OBJECT
    limine_requests.cpp
    start.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_boot)
