find_package(GTest REQUIRED)

add_library(os_lab_host_compile_settings INTERFACE)
target_compile_features(os_lab_host_compile_settings INTERFACE cxx_std_23)
target_include_directories(os_lab_host_compile_settings INTERFACE
    ${PROJECT_SOURCE_DIR}/kernel/include
)
target_link_libraries(os_lab_host_compile_settings INTERFACE
    os_lab_project_warnings
)

add_subdirectory(host_logic)
add_subdirectory(unit)
