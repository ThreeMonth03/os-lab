add_library(os_lab_kernel_runtime OBJECT
    constructors.cpp
    cxx_abi.cpp
    memops.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_runtime)
