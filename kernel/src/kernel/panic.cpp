#include "kernel/panic.hpp"

#include "kernel/halt.hpp"
#include "kernel/serial.hpp"
#include "kernel/terminal.hpp"

namespace {

void write_both(kernel::StringView value) {
    kernel::serial::write_string(value);
    if (kernel::terminal::ready()) {
        kernel::terminal::write_string(value);
    }
}

void write_both_line(kernel::StringView value) {
    kernel::serial::write_line(value);
    if (kernel::terminal::ready()) {
        kernel::terminal::write_line(value);
    }
}

void write_both_decimal(uint64_t value) {
    kernel::serial::write_decimal(value);
    if (kernel::terminal::ready()) {
        kernel::terminal::write_decimal(value);
    }
}

[[noreturn]] void finish_panic() {
    write_both_line("kernel halted");
    kernel::halt_forever();
}

} // namespace

namespace kernel {

[[noreturn]] void panic(StringView message) {
    write_both_line("");
    write_both_line("kernel panic");
    write_both("message: ");
    write_both_line(message);
    finish_panic();
}

[[noreturn]] void panic_assert(StringView condition, StringView file, int line) {
    write_both_line("");
    write_both_line("kernel assertion failed");
    write_both("condition: ");
    write_both_line(condition);
    write_both("location: ");
    write_both(file);
    write_both(":");
    write_both_decimal(static_cast<uint64_t>(line));
    write_both_line("");
    finish_panic();
}

} // namespace kernel
