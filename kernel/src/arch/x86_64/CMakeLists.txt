add_library(os_lab_kernel_arch_x86_64 OBJECT
    exception_smoke.cpp
    idt.cpp
    irq.cpp
    pic.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_arch_x86_64)
