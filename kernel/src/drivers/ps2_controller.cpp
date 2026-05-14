#include "kernel/ps2_controller.hpp"

namespace {

constexpr uint16_t kDataPort = 0x60;
constexpr uint16_t kStatusPort = 0x64;
constexpr uint16_t kCommandPort = 0x64;
constexpr uint8_t kStatusOutputReady = 0x01;
constexpr uint8_t kStatusInputFull = 0x02;
constexpr uint8_t kStatusAuxData = 0x20;
constexpr uint8_t kCommandWriteMouse = 0xd4;
constexpr uint32_t kWaitLimit = 100000;

inline uint8_t inb(uint16_t port) {
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

bool input_empty() { return (inb(kStatusPort) & kStatusInputFull) == 0; }

bool wait_input_empty() {
    for (uint32_t attempt = 0; attempt < kWaitLimit; ++attempt) {
        if (input_empty()) {
            return true;
        }
    }

    return false;
}

bool read_data_for(bool mouse, uint8_t& data) {
    const uint8_t status = inb(kStatusPort);
    if ((status & kStatusOutputReady) == 0) {
        return false;
    }

    const bool has_mouse_data = (status & kStatusAuxData) != 0;
    if (has_mouse_data != mouse) {
        return false;
    }

    data = inb(kDataPort);
    return true;
}

} // namespace

namespace kernel::ps2 {

bool read_keyboard_data(uint8_t& data) { return read_data_for(false, data); }

bool read_mouse_data(uint8_t& data) { return read_data_for(true, data); }

bool read_data(uint8_t& data, Device& device) {
    const uint8_t status = inb(kStatusPort);
    if ((status & kStatusOutputReady) == 0) {
        return false;
    }

    device = (status & kStatusAuxData) != 0 ? Device::Mouse : Device::Keyboard;
    data = inb(kDataPort);
    return true;
}

bool write_command(uint8_t command) {
    if (!wait_input_empty()) {
        return false;
    }

    outb(kCommandPort, command);
    return true;
}

bool write_data(uint8_t data) {
    if (!wait_input_empty()) {
        return false;
    }

    outb(kDataPort, data);
    return true;
}

bool write_mouse_data(uint8_t data) {
    return write_command(kCommandWriteMouse) && write_data(data);
}

void flush_output() {
    uint8_t data = 0;
    Device device = Device::Keyboard;
    while (read_data(data, device)) {
    }
}

} // namespace kernel::ps2
