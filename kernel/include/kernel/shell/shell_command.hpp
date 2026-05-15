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

[[nodiscard]] ShellCommand parse_shell_command(StringView input);

} // namespace kernel::shell
