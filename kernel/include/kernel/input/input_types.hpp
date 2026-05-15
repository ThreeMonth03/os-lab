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
    GuiSurface,
};

enum class EventTarget
{
    None,
    Shell,
    GuiSurface,
    Pointer,
};

} // namespace kernel::input
