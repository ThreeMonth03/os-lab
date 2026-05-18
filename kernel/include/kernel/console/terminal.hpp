#pragma once

#include <stdint.h>

#include "kernel/base/string_view.hpp"

namespace kernel::console::terminal
{

class ScopedUpdate
{
public:
    ScopedUpdate();
    ~ScopedUpdate();

    ScopedUpdate(const ScopedUpdate &) = delete;
    ScopedUpdate & operator=(const ScopedUpdate &) = delete;
    ScopedUpdate(ScopedUpdate &&) = delete;
    ScopedUpdate & operator=(ScopedUpdate &&) = delete;

private:
    bool active_ = false;
};

[[nodiscard]] bool init();
bool ready();
uint64_t columns();
uint64_t rows();
uint64_t cursor_column();
uint64_t cursor_row();

void clear();
void clear_cell_at(uint64_t column, uint64_t row);
void clear_row_from(uint64_t column, uint64_t row);
void draw_char_at(uint64_t column, uint64_t row, char value);
void set_row_continuation(uint64_t row, bool continuation);
void set_cursor(uint64_t column, uint64_t row);
void show_cursor();
void hide_cursor();
void write_char(char value);
void write_string(StringView value);
void write_string(const char * value);
void write_line(StringView value);
void write_line(const char * value);
void write_hex(uint64_t value);
void write_decimal(uint64_t value);

} // namespace kernel::console::terminal
