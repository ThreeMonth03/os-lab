#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'usage: %s <compile-commands-dir> <clang-tidy>\n' "$0" >&2
    exit 1
fi

compile_commands_dir=$1
clang_tidy=$2
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd -- "${script_dir}/../.." && pwd)

if [[ ! -f "${compile_commands_dir}/compile_commands.json" ]]; then
    printf 'compile_commands.json not found in: %s\n' "${compile_commands_dir}" >&2
    printf 'Run the host-side unit CMake configure before clang-tidy.\n' >&2
    exit 1
fi

mapfile -d '' sources < <(
    find \
        "${project_root}/kernel/src/text" \
        "${project_root}/tests/unit" \
        -type f \
        -name '*.cpp' \
        -print0
    printf '%s\0' \
        "${project_root}/kernel/src/arch/x86_64/paging.cpp" \
        "${project_root}/kernel/src/display/compositor.cpp" \
        "${project_root}/kernel/src/display/debug_overlay.cpp" \
        "${project_root}/kernel/src/display/display.cpp" \
        "${project_root}/kernel/src/display/display_target.cpp" \
        "${project_root}/kernel/src/display/gui_panel.cpp" \
        "${project_root}/kernel/src/display/gui_surface.cpp" \
        "${project_root}/kernel/src/display/hit_test.cpp" \
        "${project_root}/kernel/src/input/input_router.cpp" \
        "${project_root}/kernel/src/input/keyboard_decoder.cpp" \
        "${project_root}/kernel/src/input/mouse_packet_decoder.cpp" \
        "${project_root}/kernel/src/input/pointer_state.cpp" \
        "${project_root}/kernel/src/memory/heap_allocator.cpp" \
        "${project_root}/kernel/src/memory/memory_map.cpp" \
        "${project_root}/kernel/src/memory/physical_frame_allocator.cpp" \
        "${project_root}/kernel/src/memory/slab_cache.cpp" \
        "${project_root}/kernel/src/memory/slab_registry.cpp" \
        "${project_root}/kernel/src/shell/shell_command.cpp"
)

if [[ ${#sources[@]} -eq 0 ]]; then
    exit 0
fi

"${clang_tidy}" -p "${compile_commands_dir}" --quiet "${sources[@]}" \
    2> >(grep -vE '^[0-9]+ warnings generated\.$$' >&2)
