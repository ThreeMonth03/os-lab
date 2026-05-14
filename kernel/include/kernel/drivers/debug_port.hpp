#pragma once

#include "kernel/base/string_view.hpp"

namespace kernel::drivers::debug_port
{

void write_char(char value);
void write_string(StringView value);
void write_string(const char * value);

} // namespace kernel::drivers::debug_port
