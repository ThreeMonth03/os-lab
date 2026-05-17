#include "kernel/display/display_profile.hpp"

namespace kernel::display
{

void CommandProfileTracker::begin(StringView command, DisplayPipelineStats before)
{
    copy_command(command);
    before_ = make_display_stats_snapshot(before);
    pending_ = true;
}

CommandProfileDelta CommandProfileTracker::finish(DisplayPipelineStats after)
{
    if (!pending_)
    {
        return {};
    }

    pending_ = false;
    return {
        true,
        {command_, command_size_},
        display_stats_delta(before_, make_display_stats_snapshot(after)),
    };
}

void CommandProfileTracker::copy_command(StringView command)
{
    command_size_ = command.size();
    if (command_size_ > kCommandCapacity)
    {
        command_size_ = kCommandCapacity;
    }

    for (size_t index = 0; index < command_size_; ++index)
    {
        command_[index] = command[index];
    }
}

} // namespace kernel::display
