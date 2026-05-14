#include "kernel/shell_command.hpp"

namespace {

bool is_space(char value) { return value == ' ' || value == '\t'; }

kernel::StringView trim(kernel::StringView input) {
    while (!input.empty() && is_space(input.front())) {
        input.remove_prefix(1);
    }

    while (!input.empty() && is_space(input.back())) {
        input.remove_suffix(1);
    }

    return input;
}

kernel::StringView first_token(kernel::StringView input) {
    for (size_t index = 0; index < input.size(); ++index) {
        if (is_space(input[index])) {
            return input.substr(0, index);
        }
    }

    return input;
}

bool has_trailing_tokens(kernel::StringView input, kernel::StringView name) {
    kernel::StringView remaining = input.substr(name.size());
    return !trim(remaining).empty();
}

kernel::ShellCommandKind command_kind(kernel::StringView name) {
    if (name == "help") {
        return kernel::ShellCommandKind::Help;
    }
    if (name == "clear") {
        return kernel::ShellCommandKind::Clear;
    }
    if (name == "about") {
        return kernel::ShellCommandKind::About;
    }
    if (name == "input") {
        return kernel::ShellCommandKind::Input;
    }
    if (name == "mem") {
        return kernel::ShellCommandKind::Mem;
    }
    if (name == "halt") {
        return kernel::ShellCommandKind::Halt;
    }

    return kernel::ShellCommandKind::Unknown;
}

} // namespace

namespace kernel {

ShellCommand parse_shell_command(StringView input) {
    const StringView text = trim(input);
    if (text.empty()) {
        return {};
    }

    const StringView name = first_token(text);
    ShellCommandKind kind = command_kind(name);
    if (kind != ShellCommandKind::Unknown && has_trailing_tokens(text, name)) {
        kind = ShellCommandKind::Unknown;
    }

    return {kind, text, name};
}

} // namespace kernel
