#include "kernel/input/keyboard.hpp"

#include <stdint.h>

#include "kernel/arch/x86_64/pic.hpp"
#include "kernel/input/input.hpp"
#include "kernel/input/keyboard_decoder.hpp"
#include "kernel/drivers/ps2_controller.hpp"

namespace
{

constexpr uint8_t kCommandEnableKeyboard = 0xae;
constexpr uint8_t kConfigKeyboardInterrupt = 0x01;
constexpr uint8_t kConfigDisableKeyboardClock = 0x10;
constexpr uint8_t kKeyboardIrqLine = 1;

kernel::keyboard::KeyboardDecoder g_decoder;
kernel::input::DeviceMode g_input_mode = kernel::input::DeviceMode::PollingFallback;

bool read_decoded_key(kernel::keyboard::KeyEvent & event)
{
    event = {};

    uint8_t raw_scancode = 0;
    if (!kernel::drivers::ps2::read_keyboard_data(raw_scancode))
    {
        return false;
    }

    return g_decoder.decode(raw_scancode, event);
}

bool enqueue_key_event(const kernel::keyboard::KeyEvent & key)
{
    kernel::input::Event event;
    event.kind = kernel::input::EventKind::Key;
    event.source = kernel::input::EventSource::Irq;
    event.key = key;
    return kernel::input::enqueue(event);
}

} // namespace

namespace kernel::drivers::keyboard
{

bool init_irq()
{
    uint8_t config = 0;
    if (!kernel::drivers::ps2::write_command(kCommandEnableKeyboard) ||
        !kernel::drivers::ps2::read_config(config))
    {
        g_input_mode = kernel::input::DeviceMode::PollingFallback;
        return false;
    }

    config = static_cast<uint8_t>((config | kConfigKeyboardInterrupt) & ~kConfigDisableKeyboardClock);
    if (!kernel::drivers::ps2::write_config(config))
    {
        g_input_mode = kernel::input::DeviceMode::PollingFallback;
        return false;
    }

    kernel::arch::x86_64::pic::unmask(kKeyboardIrqLine);
    g_input_mode = kernel::input::DeviceMode::Irq;
    return true;
}

kernel::input::DeviceMode input_mode() { return g_input_mode; }

void handle_irq()
{
    kernel::keyboard::KeyEvent key;
    if (read_decoded_key(key))
    {
        enqueue_key_event(key);
    }
}

bool poll_key(kernel::keyboard::KeyEvent & event)
{
    return read_decoded_key(event);
}

} // namespace kernel::drivers::keyboard
