find_package(GTest REQUIRED)

add_library(os_lab_host_logic
    ${PROJECT_SOURCE_DIR}/kernel/src/kernel/display.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/kernel/history.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/kernel/keyboard_decoder.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/kernel/line_editor.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/kernel/mouse_packet_decoder.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/kernel/shell_command.cpp
    ${PROJECT_SOURCE_DIR}/kernel/src/kernel/text_console.cpp
)
target_compile_features(os_lab_host_logic PUBLIC cxx_std_23)
target_include_directories(os_lab_host_logic PUBLIC
    ${PROJECT_SOURCE_DIR}/kernel/include
)
target_compile_options(os_lab_host_logic PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(os_lab_unit_tests
    unit.cpp
)
target_link_libraries(os_lab_unit_tests PRIVATE
    os_lab_host_logic
    GTest::gtest_main
)
target_compile_options(os_lab_unit_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

include(GoogleTest)
gtest_discover_tests(os_lab_unit_tests)
