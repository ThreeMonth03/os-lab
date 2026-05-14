#include "kernel/drivers/serial.hpp"

#include <stddef.h>

namespace {

constexpr uint16_t kCom1 = 0x3f8;

inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline uint8_t inb(uint16_t port) {
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

bool can_transmit() { return (inb(kCom1 + 5) & 0x20) != 0; }

} // namespace

namespace kernel::serial {

void init() {
    outb(kCom1 + 1, 0x00);
    outb(kCom1 + 3, 0x80);
    outb(kCom1 + 0, 0x03);
    outb(kCom1 + 1, 0x00);
    outb(kCom1 + 3, 0x03);
    outb(kCom1 + 2, 0xc7);
    outb(kCom1 + 4, 0x0b);
}

void write_char(char value) {
    while (!can_transmit()) {
    }
    outb(kCom1, static_cast<uint8_t>(value));
}

void write_string(StringView value) {
    for (char character : value) {
        if (character == '\n') {
            write_char('\r');
        }
        write_char(character);
    }
}

void write_string(const char* value) { write_string(StringView(value)); }

void write_line(StringView value) {
    write_string(value);
    write_string("\n");
}

void write_line(const char* value) { write_line(StringView(value)); }

void write_hex(uint64_t value) {
    static constexpr char digits[] = "0123456789abcdef";
    write_string("0x");

    for (int shift = 60; shift >= 0; shift -= 4) {
        const auto nibble = static_cast<uint8_t>((value >> shift) & 0xf);
        write_char(digits[nibble]);
    }
}

void write_decimal(uint64_t value) {
    char buffer[21] = {};
    size_t index = 0;

    if (value == 0) {
        write_char('0');
        return;
    }

    while (value > 0) {
        buffer[index++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        write_char(buffer[--index]);
    }
}

} // namespace kernel::serial
