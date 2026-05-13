#pragma once

#include "kernel/string_view.hpp"

namespace kernel::debug_port {

void write_char(char value);
void write_string(StringView value);
void write_string(const char* value);

} // namespace kernel::debug_port
