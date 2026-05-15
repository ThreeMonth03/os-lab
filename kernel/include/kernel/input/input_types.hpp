#pragma once

namespace kernel::input
{

enum class DeviceMode
{
    PollingFallback,
    Irq,
};

enum class EventSource
{
    Unknown,
    Irq,
    PollingFallback,
};

} // namespace kernel::input
