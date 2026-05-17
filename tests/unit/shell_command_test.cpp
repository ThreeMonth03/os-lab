#include <gtest/gtest.h>
#include "kernel/shell/shell_command.hpp"
#include "test_helpers.hpp"

namespace
{

using os_lab::test::expect_text;

void expect_command(kernel::StringView input, kernel::shell::ShellCommandKind kind, kernel::StringView text, kernel::StringView name)
{
    const kernel::shell::ShellCommand command = kernel::shell::parse_shell_command(input);

    EXPECT_EQ(command.kind, kind);
    expect_text(command.text, text);
    expect_text(command.name, name);
}

void expect_command(kernel::StringView input,
                    kernel::shell::ShellCommandOptions options,
                    kernel::shell::ShellCommandKind kind,
                    kernel::StringView text,
                    kernel::StringView name)
{
    const kernel::shell::ShellCommand command = kernel::shell::parse_shell_command(input, options);

    EXPECT_EQ(command.kind, kind);
    expect_text(command.text, text);
    expect_text(command.name, name);
}

TEST(ShellCommandTest, ParsesEmptyAndWhitespaceOnlyInput)
{
    expect_command("", kernel::shell::ShellCommandKind::Empty, "", "");
    expect_command("   ", kernel::shell::ShellCommandKind::Empty, "", "");
    expect_command("\t\t", kernel::shell::ShellCommandKind::Empty, "", "");
}

TEST(ShellCommandTest, TrimsKnownCommands)
{
    expect_command("help", kernel::shell::ShellCommandKind::Help, "help", "help");
    expect_command("  clear  ", kernel::shell::ShellCommandKind::Clear, "clear", "clear");
    expect_command("\tabout\t", kernel::shell::ShellCommandKind::About, "about", "about");
    expect_command(" input ", kernel::shell::ShellCommandKind::Input, "input", "input");
    expect_command(" mem ", kernel::shell::ShellCommandKind::Mem, "mem", "mem");
    expect_command(" heap ", kernel::shell::ShellCommandKind::Heap, "heap", "heap");
    expect_command(" slab ", kernel::shell::ShellCommandKind::Slab, "slab", "slab");
    expect_command(" halt ", kernel::shell::ShellCommandKind::Halt, "halt", "halt");
}

TEST(ShellCommandTest, DisplayProfilingCommandIsOptIn)
{
    expect_command(" display ", kernel::shell::ShellCommandKind::Unknown, "display", "display");
    expect_command("display now", kernel::shell::ShellCommandKind::Unknown, "display now", "display");

    const kernel::shell::ShellCommandOptions profiling_options{true};
    expect_command(" display ", profiling_options, kernel::shell::ShellCommandKind::Display, "display", "display");
    expect_command("display now", profiling_options, kernel::shell::ShellCommandKind::Unknown, "display now", "display");
}

TEST(ShellCommandTest, ParsesUnknownCommandName)
{
    expect_command("wat", kernel::shell::ShellCommandKind::Unknown, "wat", "wat");
    expect_command("  wat  ", kernel::shell::ShellCommandKind::Unknown, "wat", "wat");
    expect_command("wat now", kernel::shell::ShellCommandKind::Unknown, "wat now", "wat");
}

TEST(ShellCommandTest, TreatsKnownCommandsWithArgumentsAsUnknown)
{
    expect_command("help now", kernel::shell::ShellCommandKind::Unknown, "help now", "help");
    expect_command("clear now", kernel::shell::ShellCommandKind::Unknown, "clear now", "clear");
    expect_command("about now", kernel::shell::ShellCommandKind::Unknown, "about now", "about");
    expect_command("input now", kernel::shell::ShellCommandKind::Unknown, "input now", "input");
    expect_command("mem now", kernel::shell::ShellCommandKind::Unknown, "mem now", "mem");
    expect_command("heap now", kernel::shell::ShellCommandKind::Unknown, "heap now", "heap");
    expect_command("slab now", kernel::shell::ShellCommandKind::Unknown, "slab now", "slab");
    expect_command("halt now", kernel::shell::ShellCommandKind::Unknown, "halt now", "halt");
}

} // namespace
