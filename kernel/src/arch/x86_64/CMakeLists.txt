add_library(os_lab_kernel_arch_x86_64 OBJECT
    active_paging.cpp
    exception_smoke.cpp
    idt.cpp
    irq.cpp
    paging.cpp
    paging_init.cpp
    pic.cpp
    pit.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_arch_x86_64)
