#include "kernel/drivers/debug_port.hpp"

namespace {

constexpr unsigned short kDebugPort = 0xe9;

inline void outb(unsigned short port, unsigned char value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

} // namespace

namespace kernel::debug_port {

void write_char(char value) { outb(kDebugPort, static_cast<unsigned char>(value)); }

void write_string(StringView value) {
    for (char character : value) {
        write_char(character);
    }
}

void write_string(const char* value) { write_string(StringView(value)); }

} // namespace kernel::debug_port
