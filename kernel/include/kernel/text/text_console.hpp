#pragma once

#include <stdint.h>

namespace kernel::text
{

struct ConsoleCell
{
    uint64_t column = 0;
    uint64_t row = 0;
};

enum class TextConsoleAction
{
    None,
    DrawGlyph,
    ClearCell,
};

enum class TextConsoleLineBreak
{
    None,
    Hard,
    SoftWrap,
};

struct TextConsoleUpdate
{
    TextConsoleAction action = TextConsoleAction::None;
    ConsoleCell cell = {};
    char glyph = '\0';
    bool scroll = false;
    TextConsoleLineBreak line_break = TextConsoleLineBreak::None;
    uint64_t line_break_row = 0;
};

class TextConsole
{
public:
    TextConsole() = default;
    TextConsole(uint64_t columns, uint64_t rows);

    void reset(uint64_t columns, uint64_t rows);
    void clear();

    bool ready() const { return columns_ > 0 && rows_ > 0; }
    uint64_t columns() const { return columns_; }
    uint64_t rows() const { return rows_; }
    uint64_t cursor_column() const { return cursor_column_; }
    uint64_t cursor_row() const { return cursor_row_; }
    ConsoleCell cursor() const { return {cursor_column_, cursor_row_}; }

    [[nodiscard]] TextConsoleUpdate write_char(char value);
    [[nodiscard]] TextConsoleUpdate newline();
    void carriage_return();
    [[nodiscard]] TextConsoleUpdate backspace();
    void set_cursor(uint64_t column, uint64_t row);

private:
    uint64_t columns_ = 0;
    uint64_t rows_ = 0;
    uint64_t cursor_column_ = 0;
    uint64_t cursor_row_ = 0;
}; // end class TextConsole

} // namespace kernel::text
