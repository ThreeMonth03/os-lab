add_library(os_lab_kernel_arch_x86_64 OBJECT
    idt.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_arch_x86_64)
