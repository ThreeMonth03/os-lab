#pragma once

#include <stddef.h>

#include "kernel/base/string_view.hpp"
#include "kernel/display/display_stats.hpp"

namespace kernel::display
{

struct CommandProfileDelta
{
    bool valid = false;
    StringView command;
    DisplayPipelineStats delta;
}; // end struct CommandProfileDelta

class CommandProfileTracker
{
public:
    void begin(StringView command, DisplayPipelineStats before);
    CommandProfileDelta finish(DisplayPipelineStats after);
    bool pending() const { return pending_; }

private:
    static constexpr size_t kCommandCapacity = 32;

    void copy_command(StringView command);

    char command_[kCommandCapacity] = {};
    size_t command_size_ = 0;
    DisplayStatsSnapshot before_;
    bool pending_ = false;
}; // end class CommandProfileTracker

} // namespace kernel::display
