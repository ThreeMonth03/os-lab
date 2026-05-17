#pragma once

#include "kernel/base/string_view.hpp"
#include "kernel/display/display_stats.hpp"

namespace kernel::debug
{

void write_display_profile_delta(StringView command, display::DisplayPipelineStats delta);

} // namespace kernel::debug
