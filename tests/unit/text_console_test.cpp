#include <stdint.h>
#include <gtest/gtest.h>
#include "kernel/text/text_console.hpp"

namespace
{

void expect_cell(kernel::text::ConsoleCell actual, uint64_t column, uint64_t row)
{
    EXPECT_EQ(actual.column, column);
    EXPECT_EQ(actual.row, row);
}

TEST(TextConsoleTest, WritesCharacterAndAdvancesCursor)
{
    kernel::text::TextConsole console(4, 3);

    const kernel::text::TextConsoleUpdate update = console.write_char('x');

    EXPECT_EQ(update.action, kernel::text::TextConsoleAction::DrawGlyph);
    expect_cell(update.cell, 0, 0);
    EXPECT_EQ(update.glyph, 'x');
    EXPECT_FALSE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 1u);
    EXPECT_EQ(console.cursor_row(), 0u);
}

TEST(TextConsoleTest, NewlineMovesToNextRow)
{
    kernel::text::TextConsole console(4, 3);
    console.set_cursor(2, 0);

    const kernel::text::TextConsoleUpdate update = console.newline();

    EXPECT_EQ(update.action, kernel::text::TextConsoleAction::None);
    EXPECT_FALSE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 1u);
}

TEST(TextConsoleTest, NewlineAtBottomRequestsScroll)
{
    kernel::text::TextConsole console(4, 3);
    console.set_cursor(2, 2);

    const kernel::text::TextConsoleUpdate update = console.newline();

    EXPECT_TRUE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 2u);
}

TEST(TextConsoleTest, WritingLastBottomCellRequestsScrollAfterDraw)
{
    kernel::text::TextConsole console(4, 3);
    console.set_cursor(3, 2);

    const kernel::text::TextConsoleUpdate update = console.write_char('z');

    EXPECT_EQ(update.action, kernel::text::TextConsoleAction::DrawGlyph);
    expect_cell(update.cell, 3, 2);
    EXPECT_EQ(update.glyph, 'z');
    EXPECT_TRUE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 2u);
}

TEST(TextConsoleTest, ClearResetsCursor)
{
    kernel::text::TextConsole console(4, 3);
    console.set_cursor(3, 2);

    console.clear();

    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 0u);
}

TEST(TextConsoleTest, SetCursorClampsToBounds)
{
    kernel::text::TextConsole console(4, 3);

    console.set_cursor(99, 99);

    EXPECT_EQ(console.cursor_column(), 3u);
    EXPECT_EQ(console.cursor_row(), 2u);
}

TEST(TextConsoleTest, BackspaceClearsPreviousCell)
{
    kernel::text::TextConsole console(4, 3);
    console.set_cursor(2, 1);

    const kernel::text::TextConsoleUpdate update = console.backspace();

    EXPECT_EQ(update.action, kernel::text::TextConsoleAction::ClearCell);
    expect_cell(update.cell, 1, 1);
    EXPECT_EQ(console.cursor_column(), 1u);
    EXPECT_EQ(console.cursor_row(), 1u);
}

TEST(TextConsoleTest, BackspaceAtLineStartDoesNothing)
{
    kernel::text::TextConsole console(4, 3);
    console.set_cursor(0, 1);

    const kernel::text::TextConsoleUpdate update = console.backspace();

    EXPECT_EQ(update.action, kernel::text::TextConsoleAction::None);
    EXPECT_FALSE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 1u);
}

} // namespace
