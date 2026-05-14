cmake_minimum_required(VERSION 3.20)

project(os_lab_kernel LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "This project is configured for clang/clang++.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(kernel
    kernel/src/boot/limine_requests.cpp
    kernel/src/boot/start.cpp
    kernel/src/drivers/debug_port.cpp
    kernel/src/drivers/framebuffer.cpp
    kernel/src/drivers/keyboard.cpp
    kernel/src/drivers/serial.cpp
    kernel/src/drivers/terminal.cpp
    kernel/src/kernel/history.cpp
    kernel/src/kernel/line_editor.cpp
    kernel/src/kernel/main.cpp
    kernel/src/kernel/shell.cpp
    kernel/src/runtime/constructors.cpp
    kernel/src/runtime/memory.cpp
)

target_include_directories(kernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/limine/include
)

target_compile_options(kernel PRIVATE
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

target_link_options(kernel PRIVATE
    -nostdlib
    -static
    -fuse-ld=lld
    -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/kernel/linker.ld
    -Wl,--build-id=none
    -Wl,--gc-sections
    -Wl,-z,max-page-size=0x1000
)

set_target_properties(kernel PROPERTIES
    OUTPUT_NAME kernel
    SUFFIX .elf
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/artifacts
)
