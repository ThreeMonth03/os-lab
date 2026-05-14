if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The freestanding kernel target requires clang/clang++.")
endif()

set(OS_LAB_EXCEPTION_SMOKE "none" CACHE STRING
    "Debug-only exception smoke trigger: none, invalid_opcode, page_fault, divide_error")
set_property(CACHE OS_LAB_EXCEPTION_SMOKE PROPERTY STRINGS
    none invalid_opcode page_fault divide_error)
option(OS_LAB_TIMER_SMOKE "Debug-only PIT timer smoke trigger" OFF)
option(OS_LAB_PAGING_SMOKE "Debug-only active paging smoke trigger" OFF)
option(OS_LAB_HEAP_SMOKE "Debug-only kernel heap smoke trigger" OFF)

set(OS_LAB_EXCEPTION_SMOKE_VALUE 0)
if(OS_LAB_EXCEPTION_SMOKE STREQUAL "none")
    set(OS_LAB_EXCEPTION_SMOKE_VALUE 0)
elseif(OS_LAB_EXCEPTION_SMOKE STREQUAL "invalid_opcode")
    set(OS_LAB_EXCEPTION_SMOKE_VALUE 1)
elseif(OS_LAB_EXCEPTION_SMOKE STREQUAL "page_fault")
    set(OS_LAB_EXCEPTION_SMOKE_VALUE 2)
elseif(OS_LAB_EXCEPTION_SMOKE STREQUAL "divide_error")
    set(OS_LAB_EXCEPTION_SMOKE_VALUE 3)
else()
    message(FATAL_ERROR
        "OS_LAB_EXCEPTION_SMOKE must be one of: none, invalid_opcode, page_fault, divide_error")
endif()

set(OS_LAB_TIMER_SMOKE_VALUE 0)
if(OS_LAB_TIMER_SMOKE)
    set(OS_LAB_TIMER_SMOKE_VALUE 1)
endif()

set(OS_LAB_PAGING_SMOKE_VALUE 0)
if(OS_LAB_PAGING_SMOKE)
    set(OS_LAB_PAGING_SMOKE_VALUE 1)
endif()

set(OS_LAB_HEAP_SMOKE_VALUE 0)
if(OS_LAB_HEAP_SMOKE)
    set(OS_LAB_HEAP_SMOKE_VALUE 1)
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
)
target_compile_definitions(os_lab_kernel_compile_settings INTERFACE
    OS_LAB_EXCEPTION_SMOKE=${OS_LAB_EXCEPTION_SMOKE_VALUE}
    OS_LAB_HEAP_SMOKE=${OS_LAB_HEAP_SMOKE_VALUE}
    OS_LAB_PAGING_SMOKE=${OS_LAB_PAGING_SMOKE_VALUE}
    OS_LAB_TIMER_SMOKE=${OS_LAB_TIMER_SMOKE_VALUE}
)
target_link_libraries(os_lab_kernel_compile_settings INTERFACE
    os_lab_project_warnings
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

add_subdirectory(src/arch/x86_64)
add_subdirectory(src/boot)
add_subdirectory(src/console)
add_subdirectory(src/debug)
add_subdirectory(src/display)
add_subdirectory(src/drivers)
add_subdirectory(src/input)
add_subdirectory(src/kernel)
add_subdirectory(src/memory)
add_subdirectory(src/runtime)
add_subdirectory(src/shell)
add_subdirectory(src/text)

add_executable(kernel
    $<TARGET_OBJECTS:os_lab_kernel_arch_x86_64>
    $<TARGET_OBJECTS:os_lab_kernel_boot>
    $<TARGET_OBJECTS:os_lab_kernel_console>
    $<TARGET_OBJECTS:os_lab_kernel_debug>
    $<TARGET_OBJECTS:os_lab_kernel_display>
    $<TARGET_OBJECTS:os_lab_kernel_drivers>
    $<TARGET_OBJECTS:os_lab_kernel_input>
    $<TARGET_OBJECTS:os_lab_kernel_core>
    $<TARGET_OBJECTS:os_lab_kernel_memory>
    $<TARGET_OBJECTS:os_lab_kernel_runtime>
    $<TARGET_OBJECTS:os_lab_kernel_shell>
    $<TARGET_OBJECTS:os_lab_kernel_text>
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
