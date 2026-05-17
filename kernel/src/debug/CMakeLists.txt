add_library(os_lab_kernel_debug OBJECT
    display_profile.cpp
    heap_smoke.cpp
    paging_smoke.cpp
    slab_smoke.cpp
    timer_smoke.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_debug)
