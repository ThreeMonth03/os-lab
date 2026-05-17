#include "kernel/shell/shell_command.hpp"

namespace
{

bool is_space(char value) { return value == ' ' || value == '\t'; }

kernel::StringView trim(kernel::StringView input)
{
    while (!input.empty() && is_space(input.front()))
    {
        input.remove_prefix(1);
    }

    while (!input.empty() && is_space(input.back()))
    {
        input.remove_suffix(1);
    }

    return input;
}

kernel::StringView first_token(kernel::StringView input)
{
    for (size_t index = 0; index < input.size(); ++index)
    {
        if (is_space(input[index]))
        {
            return input.substr(0, index);
        }
    }

    return input;
}

bool has_trailing_tokens(kernel::StringView input, kernel::StringView name)
{
    kernel::StringView remaining = input.substr(name.size());
    return !trim(remaining).empty();
}

#ifndef OS_LAB_DISPLAY_PROFILING
#define OS_LAB_DISPLAY_PROFILING 0
#endif

kernel::shell::ShellCommandOptions default_options()
{
    return {OS_LAB_DISPLAY_PROFILING != 0};
}

kernel::shell::ShellCommandKind command_kind(kernel::StringView name, kernel::shell::ShellCommandOptions options)
{
    if (name == "help")
    {
        return kernel::shell::ShellCommandKind::Help;
    }
    if (name == "clear")
    {
        return kernel::shell::ShellCommandKind::Clear;
    }
    if (name == "about")
    {
        return kernel::shell::ShellCommandKind::About;
    }
    if (name == "input")
    {
        return kernel::shell::ShellCommandKind::Input;
    }
    if (options.display_profiling_enabled && name == "display")
    {
        return kernel::shell::ShellCommandKind::Display;
    }
    if (name == "mem")
    {
        return kernel::shell::ShellCommandKind::Mem;
    }
    if (name == "heap")
    {
        return kernel::shell::ShellCommandKind::Heap;
    }
    if (name == "slab")
    {
        return kernel::shell::ShellCommandKind::Slab;
    }
    if (name == "halt")
    {
        return kernel::shell::ShellCommandKind::Halt;
    }

    return kernel::shell::ShellCommandKind::Unknown;
}

} // namespace

namespace kernel::shell
{

ShellCommand parse_shell_command(StringView input)
{
    return parse_shell_command(input, default_options());
}

ShellCommand parse_shell_command(StringView input, ShellCommandOptions options)
{
    const StringView text = trim(input);
    if (text.empty())
    {
        return {};
    }

    const StringView name = first_token(text);
    ShellCommandKind kind = command_kind(name, options);
    if (kind != ShellCommandKind::Unknown && has_trailing_tokens(text, name))
    {
        kind = ShellCommandKind::Unknown;
    }

    return {kind, text, name};
}

} // namespace kernel::shell
