#include <gtest/gtest.h>
#include "kernel/shell/shell_command.hpp"
#include "test_helpers.hpp"

namespace
{

using os_lab::test::expect_text;

void expect_command(kernel::StringView input, kernel::ShellCommandKind kind, kernel::StringView text, kernel::StringView name)
{
    const kernel::ShellCommand command = kernel::parse_shell_command(input);

    EXPECT_EQ(command.kind, kind);
    expect_text(command.text, text);
    expect_text(command.name, name);
}

TEST(ShellCommandTest, ParsesEmptyAndWhitespaceOnlyInput)
{
    expect_command("", kernel::ShellCommandKind::Empty, "", "");
    expect_command("   ", kernel::ShellCommandKind::Empty, "", "");
    expect_command("\t\t", kernel::ShellCommandKind::Empty, "", "");
}

TEST(ShellCommandTest, TrimsKnownCommands)
{
    expect_command("help", kernel::ShellCommandKind::Help, "help", "help");
    expect_command("  clear  ", kernel::ShellCommandKind::Clear, "clear", "clear");
    expect_command("\tabout\t", kernel::ShellCommandKind::About, "about", "about");
    expect_command(" input ", kernel::ShellCommandKind::Input, "input", "input");
    expect_command(" mem ", kernel::ShellCommandKind::Mem, "mem", "mem");
    expect_command(" heap ", kernel::ShellCommandKind::Heap, "heap", "heap");
    expect_command(" halt ", kernel::ShellCommandKind::Halt, "halt", "halt");
}

TEST(ShellCommandTest, ParsesUnknownCommandName)
{
    expect_command("wat", kernel::ShellCommandKind::Unknown, "wat", "wat");
    expect_command("  wat  ", kernel::ShellCommandKind::Unknown, "wat", "wat");
    expect_command("wat now", kernel::ShellCommandKind::Unknown, "wat now", "wat");
}

TEST(ShellCommandTest, TreatsKnownCommandsWithArgumentsAsUnknown)
{
    expect_command("help now", kernel::ShellCommandKind::Unknown, "help now", "help");
    expect_command("clear now", kernel::ShellCommandKind::Unknown, "clear now", "clear");
    expect_command("about now", kernel::ShellCommandKind::Unknown, "about now", "about");
    expect_command("input now", kernel::ShellCommandKind::Unknown, "input now", "input");
    expect_command("mem now", kernel::ShellCommandKind::Unknown, "mem now", "mem");
    expect_command("heap now", kernel::ShellCommandKind::Unknown, "heap now", "heap");
    expect_command("halt now", kernel::ShellCommandKind::Unknown, "halt now", "halt");
}

} // namespace
