#pragma once

#include "kernel/base/string_view.hpp"

namespace kernel
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
};

[[nodiscard]] ShellCommand parse_shell_command(StringView input);

} // namespace kernel
