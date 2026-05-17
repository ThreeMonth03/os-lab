#pragma once

#include "kernel/base/string_view.hpp"
#include "kernel/display/display_stats.hpp"

namespace kernel::debug
{

void write_display_profile_delta(StringView command, display::DisplayPipelineStats delta);
void begin_display_profile_command(StringView command, display::DisplayPipelineStats before);
void finish_display_profile_command(display::DisplayPipelineStats after);

} // namespace kernel::debug
