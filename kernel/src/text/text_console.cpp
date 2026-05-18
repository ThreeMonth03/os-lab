#include "kernel/text/text_console.hpp"

namespace kernel::text
{

TextConsole::TextConsole(uint64_t columns, uint64_t rows) { reset(columns, rows); }

void TextConsole::reset(uint64_t columns, uint64_t rows)
{
    columns_ = columns;
    rows_ = rows;
    clear();
}

void TextConsole::clear()
{
    cursor_column_ = 0;
    cursor_row_ = 0;
}

TextConsoleUpdate TextConsole::write_char(char value)
{
    if (!ready())
    {
        return {};
    }

    TextConsoleUpdate update = {};
    update.action = TextConsoleAction::DrawGlyph;
    update.cell = cursor();
    update.glyph = value;

    ++cursor_column_;
    if (cursor_column_ >= columns_)
    {
        const TextConsoleUpdate newline_update = newline();
        update.scroll = newline_update.scroll;
        update.line_break = TextConsoleLineBreak::SoftWrap;
        update.line_break_row = newline_update.line_break_row;
    }

    return update;
}

TextConsoleUpdate TextConsole::newline()
{
    if (!ready())
    {
        return {};
    }

    cursor_column_ = 0;
    if (cursor_row_ + 1 >= rows_)
    {
        TextConsoleUpdate update = {};
        update.scroll = true;
        update.line_break = TextConsoleLineBreak::Hard;
        update.line_break_row = cursor_row_;
        return update;
    }

    ++cursor_row_;
    TextConsoleUpdate update = {};
    update.line_break = TextConsoleLineBreak::Hard;
    update.line_break_row = cursor_row_;
    return update;
}

void TextConsole::carriage_return()
{
    if (!ready())
    {
        return;
    }

    cursor_column_ = 0;
}

TextConsoleUpdate TextConsole::backspace()
{
    if (!ready() || cursor_column_ == 0)
    {
        return {};
    }

    --cursor_column_;

    TextConsoleUpdate update = {};
    update.action = TextConsoleAction::ClearCell;
    update.cell = cursor();
    return update;
}

void TextConsole::set_cursor(uint64_t column, uint64_t row)
{
    if (!ready())
    {
        cursor_column_ = 0;
        cursor_row_ = 0;
        return;
    }

    cursor_column_ = column < columns_ ? column : columns_ - 1;
    cursor_row_ = row < rows_ ? row : rows_ - 1;
}

} // namespace kernel::text
