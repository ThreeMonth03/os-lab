#pragma once

#include "kernel/base/string_view.hpp"

namespace kernel::shell
{

enum class ShellCommandKind
{
    Empty,
    Help,
    Clear,
    About,
    Input,
    Display,
    Mem,
    Heap,
    Slab,
    Halt,
    Unknown,
};

struct ShellCommand
{
    ShellCommandKind kind = ShellCommandKind::Empty;
    StringView text;
    StringView name;
}; // end struct ShellCommand

struct ShellCommandOptions
{
    bool display_profiling_enabled = false;
}; // end struct ShellCommandOptions

[[nodiscard]] ShellCommand parse_shell_command(StringView input);
[[nodiscard]] ShellCommand parse_shell_command(StringView input, ShellCommandOptions options);

} // namespace kernel::shell
