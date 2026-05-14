#pragma once

#include <stdint.h>

#include "kernel/base/string_view.hpp"

namespace kernel::drivers::serial {

void init();
void write_char(char value);
void write_string(StringView value);
void write_string(const char* value);
void write_line(StringView value);
void write_line(const char* value);
void write_hex(uint64_t value);
void write_decimal(uint64_t value);

} // namespace kernel::drivers::serial
