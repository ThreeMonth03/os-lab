cmake_minimum_required(VERSION 3.20)

project(os_lab LANGUAGES CXX)

option(OS_LAB_BUILD_KERNEL "Build the freestanding kernel image" ON)
option(OS_LAB_BUILD_TESTS "Build host-side unit tests" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(OS_LAB_BUILD_KERNEL)
    add_subdirectory(kernel)
endif()

if(OS_LAB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
