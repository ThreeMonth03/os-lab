add_library(os_lab_kernel_memory OBJECT
    memory.cpp
    memory_map.cpp
    physical_frame_allocator.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_memory)
