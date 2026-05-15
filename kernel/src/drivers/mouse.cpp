#include "kernel/input/mouse.hpp"

#include "kernel/input/mouse_packet_decoder.hpp"
#include "kernel/drivers/ps2_controller.hpp"

namespace
{

constexpr uint8_t kCommandEnableAuxiliary = 0xa8;
constexpr uint8_t kConfigDisableMouseClock = 0x20;
constexpr uint8_t kMouseSetDefaults = 0xf6;
constexpr uint8_t kMouseEnableDataReporting = 0xf4;
constexpr uint8_t kMouseAck = 0xfa;
constexpr uint32_t kResponseWaitLimit = 100000;

kernel::mouse::MousePacketDecoder g_decoder;
bool g_ready = false;
kernel::mouse::InputMode g_input_mode = kernel::mouse::InputMode::PollingFallback;

bool wait_for_mouse_ack()
{
    uint8_t data = 0;
    kernel::drivers::ps2::Device device = kernel::drivers::ps2::Device::Keyboard;

    for (uint32_t attempt = 0; attempt < kResponseWaitLimit; ++attempt)
    {
        if (!kernel::drivers::ps2::read_data(data, device))
        {
            continue;
        }
        if (device == kernel::drivers::ps2::Device::Mouse)
        {
            return data == kMouseAck;
        }
    }

    return false;
}

bool send_mouse_command(uint8_t command)
{
    return kernel::drivers::ps2::write_mouse_data(command) && wait_for_mouse_ack();
}

} // namespace

namespace kernel::mouse
{

bool init()
{
    g_ready = false;
    g_input_mode = InputMode::PollingFallback;
    g_decoder.reset();

    kernel::drivers::ps2::flush_output();
    if (!kernel::drivers::ps2::write_command(kCommandEnableAuxiliary))
    {
        return false;
    }

    uint8_t config = 0;
    if (kernel::drivers::ps2::read_config(config))
    {
        config = static_cast<uint8_t>(config & ~kConfigDisableMouseClock);
        if (!kernel::drivers::ps2::write_config(config))
        {
            return false;
        }
    }

    kernel::drivers::ps2::flush_output();
    if (!send_mouse_command(kMouseSetDefaults))
    {
        return false;
    }
    if (!send_mouse_command(kMouseEnableDataReporting))
    {
        return false;
    }

    g_ready = true;
    return true;
}

bool ready() { return g_ready; }

InputMode input_mode() { return g_input_mode; }

bool poll(MouseEvent & event)
{
    event = {};
    if (!g_ready || g_input_mode == InputMode::Irq)
    {
        return false;
    }

    uint8_t data = 0;
    if (!kernel::drivers::ps2::read_mouse_data(data))
    {
        return false;
    }

    MousePacket packet;
    if (!g_decoder.decode(data, packet))
    {
        return false;
    }

    event.delta_x = packet.delta_x;
    event.delta_y = packet.delta_y;
    event.left_button = packet.left_button;
    event.right_button = packet.right_button;
    event.middle_button = packet.middle_button;
    event.x_overflow = packet.x_overflow;
    event.y_overflow = packet.y_overflow;
    return true;
}

} // namespace kernel::mouse
