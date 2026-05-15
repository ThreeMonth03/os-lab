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

enum class InputFocus
{
    None,
    Shell,
};

enum class EventTarget
{
    None,
    Shell,
    Pointer,
};

} // namespace kernel::input
