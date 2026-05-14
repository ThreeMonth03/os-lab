add_library(os_lab_kernel_shell OBJECT
    shell.cpp
    shell_command.cpp
    shell_command_executor.cpp
)
os_lab_configure_kernel_object(os_lab_kernel_shell)
