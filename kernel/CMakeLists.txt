if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The freestanding kernel target requires clang/clang++.")
endif()

add_library(os_lab_kernel_compile_settings INTERFACE)
target_compile_features(os_lab_kernel_compile_settings INTERFACE cxx_std_23)
target_include_directories(os_lab_kernel_compile_settings INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/vendor/limine/include
)
target_compile_options(os_lab_kernel_compile_settings INTERFACE
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-stack-protector
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -fno-pic
    -fno-pie
    -ffunction-sections
    -fdata-sections
    -mno-red-zone
    -mcmodel=kernel
    -mno-mmx
    -mno-sse
    -mno-sse2
    -msoft-float
    -nostdinc++
    -Wall
    -Wextra
    -Wpedantic
)

add_library(os_lab_kernel_link_settings INTERFACE)
target_link_options(os_lab_kernel_link_settings INTERFACE
    -nostdlib
    -static
    -fuse-ld=lld
    -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/linker.ld
    -Wl,--build-id=none
    -Wl,--gc-sections
    -Wl,-z,max-page-size=0x1000
)

function(os_lab_configure_kernel_object target)
    target_link_libraries(${target} PRIVATE os_lab_kernel_compile_settings)
endfunction()

add_subdirectory(src/boot)
add_subdirectory(src/drivers)
add_subdirectory(src/kernel)
add_subdirectory(src/runtime)

add_executable(kernel
    $<TARGET_OBJECTS:os_lab_kernel_boot>
    $<TARGET_OBJECTS:os_lab_kernel_drivers>
    $<TARGET_OBJECTS:os_lab_kernel_logic>
    $<TARGET_OBJECTS:os_lab_kernel_core>
    $<TARGET_OBJECTS:os_lab_kernel_runtime>
)
target_link_libraries(kernel PRIVATE
    os_lab_kernel_compile_settings
    os_lab_kernel_link_settings
)
set_target_properties(kernel PROPERTIES
    OUTPUT_NAME kernel
    SUFFIX .elf
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/artifacts
)
