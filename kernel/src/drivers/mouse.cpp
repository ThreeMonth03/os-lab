#include "kernel/drivers/mouse.hpp"

#include "kernel/arch/x86_64/pic.hpp"
#include "kernel/input/input.hpp"
#include "kernel/input/mouse_packet_decoder.hpp"
#include "kernel/drivers/ps2_controller.hpp"

namespace
{

constexpr uint8_t kCommandEnableAuxiliary = 0xa8;
constexpr uint8_t kConfigMouseInterrupt = 0x02;
constexpr uint8_t kConfigDisableMouseClock = 0x20;
constexpr uint8_t kMouseSetDefaults = 0xf6;
constexpr uint8_t kMouseEnableDataReporting = 0xf4;
constexpr uint8_t kMouseAck = 0xfa;
constexpr uint8_t kMouseIrqLine = 12;
constexpr uint32_t kResponseWaitLimit = 100000;

kernel::input::mouse::MousePacketDecoder g_decoder;
bool g_ready = false;
kernel::input::DeviceMode g_input_mode = kernel::input::DeviceMode::PollingFallback;

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

bool configure_controller(bool & irq_configured)
{
    irq_configured = false;

    uint8_t config = 0;
    if (!kernel::drivers::ps2::read_config(config))
    {
        return false;
    }

    config = static_cast<uint8_t>((config | kConfigMouseInterrupt) & ~kConfigDisableMouseClock);
    if (!kernel::drivers::ps2::write_config(config))
    {
        return false;
    }

    irq_configured = true;
    return true;
}

void enqueue_mouse_event(const kernel::input::mouse::MousePacket & packet)
{
    kernel::input::Event event;
    event.kind = kernel::input::EventKind::MouseMove;
    event.source = kernel::input::EventSource::Irq;
    event.mouse_move.delta_x = packet.delta_x;
    event.mouse_move.delta_y = packet.delta_y;
    event.mouse_move.left_button = packet.left_button;
    event.mouse_move.right_button = packet.right_button;
    event.mouse_move.middle_button = packet.middle_button;
    event.mouse_move.x_overflow = packet.x_overflow;
    event.mouse_move.y_overflow = packet.y_overflow;
    kernel::input::enqueue(event);
}

} // namespace

namespace kernel::drivers::mouse
{

bool init()
{
    g_ready = false;
    g_input_mode = kernel::input::DeviceMode::PollingFallback;
    g_decoder.reset();

    kernel::drivers::ps2::flush_output();
    if (!kernel::drivers::ps2::write_command(kCommandEnableAuxiliary))
    {
        return false;
    }

    bool irq_configured = false;
    configure_controller(irq_configured);

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
    if (irq_configured)
    {
        kernel::arch::x86_64::pic::unmask(kMouseIrqLine);
        g_input_mode = kernel::input::DeviceMode::Irq;
    }
    return true;
}

kernel::input::DeviceMode input_mode() { return g_input_mode; }

void handle_irq()
{
    if (!g_ready)
    {
        return;
    }

    uint8_t data = 0;
    if (!kernel::drivers::ps2::read_mouse_data(data))
    {
        return;
    }

    kernel::input::mouse::MousePacket packet;
    if (g_decoder.decode(data, packet))
    {
        enqueue_mouse_event(packet);
    }
}

bool poll(kernel::input::mouse::MouseEvent & event)
{
    event = {};
    if (!g_ready || g_input_mode == kernel::input::DeviceMode::Irq)
    {
        return false;
    }

    uint8_t data = 0;
    if (!kernel::drivers::ps2::read_mouse_data(data))
    {
        return false;
    }

    kernel::input::mouse::MousePacket packet;
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

} // namespace kernel::drivers::mouse
