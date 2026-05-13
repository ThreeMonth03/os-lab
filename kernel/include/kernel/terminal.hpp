#pragma once

#include "kernel/string_view.hpp"

namespace kernel::terminal {

[[nodiscard]] bool init();
[[nodiscard]] bool ready();

void clear();
void write_char(char value);
void write_string(StringView value);
void write_string(const char* value);
void write_line(StringView value);
void write_line(const char* value);

} // namespace kernel::terminal
