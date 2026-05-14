#pragma once

#include <stdint.h>

#include "kernel/string_view.hpp"

namespace kernel::terminal {

[[nodiscard]] bool init();
[[nodiscard]] bool ready();
[[nodiscard]] uint64_t columns();
[[nodiscard]] uint64_t rows();
[[nodiscard]] uint64_t cursor_column();
[[nodiscard]] uint64_t cursor_row();

void clear();
void clear_row_from(uint64_t column, uint64_t row);
void set_cursor(uint64_t column, uint64_t row);
void show_cursor();
void hide_cursor();
void write_char(char value);
void write_string(StringView value);
void write_string(const char* value);
void write_line(StringView value);
void write_line(const char* value);

} // namespace kernel::terminal
