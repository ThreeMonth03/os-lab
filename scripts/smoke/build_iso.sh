#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    printf 'usage: %s <demo|exception|timer|paging|heap|slab|terminal-app>\n' "$0" >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd -- "${script_dir}/../.." && pwd)
create_iso="${project_root}/scripts/build/create_iso.sh"
kind=$1

build_root=${BUILD_DIR:-"${project_root}/build"}
cmake_bin=${CMAKE:-cmake}
generator=${GENERATOR:-Ninja}
toolchain_file=${TOOLCHAIN_FILE:-"${project_root}/cmake/toolchains/x86_64-none-clang.cmake"}
build_type=${CMAKE_BUILD_TYPE:-Debug}
gui_panel_visible=${GUI_PANEL_VISIBLE:-OFF}
display_profiling=${DISPLAY_PROFILING:-OFF}
display_profile_script=${DISPLAY_PROFILE_SCRIPT:-OFF}
terminal_window_chrome=${TERMINAL_WINDOW_CHROME:-OFF}
terminal_window_interaction=${TERMINAL_WINDOW_INTERACTION:-OFF}

cmake_options=(
    -DCMAKE_BUILD_TYPE="${build_type}"
    -DCMAKE_TOOLCHAIN_FILE="${toolchain_file}"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DOS_LAB_GUI_PANEL_VISIBLE="${gui_panel_visible}"
    -DOS_LAB_DISPLAY_PROFILING="${display_profiling}"
    -DOS_LAB_DISPLAY_PROFILE_SCRIPT="${display_profile_script}"
    -DOS_LAB_TERMINAL_WINDOW_CHROME="${terminal_window_chrome}"
    -DOS_LAB_TERMINAL_WINDOW_INTERACTION="${terminal_window_interaction}"
)

case "${kind}" in
    demo)
        build_dir="${build_root}"
        ;;
    exception)
        exception_smoke=${EXCEPTION_SMOKE:-invalid_opcode}
        build_dir="${build_root}/exception-smoke/${exception_smoke}"
        cmake_options+=(-DOS_LAB_EXCEPTION_SMOKE="${exception_smoke}")
        ;;
    timer)
        build_dir="${build_root}/timer-smoke"
        cmake_options+=(-DOS_LAB_TIMER_SMOKE=ON)
        ;;
    paging)
        build_dir="${build_root}/paging-smoke"
        cmake_options+=(-DOS_LAB_PAGING_SMOKE=ON)
        ;;
    heap)
        build_dir="${build_root}/heap-smoke"
        cmake_options+=(-DOS_LAB_HEAP_SMOKE=ON)
        ;;
    slab)
        build_dir="${build_root}/slab-smoke"
        cmake_options+=(-DOS_LAB_SLAB_SMOKE=ON)
        ;;
    terminal-app)
        build_dir="${build_root}/terminal-app-smoke"
        cmake_options+=(-DOS_LAB_TERMINAL_APP_LIFECYCLE_SMOKE=ON)
        ;;
    *)
        printf 'unknown smoke kind: %s\n' "${kind}" >&2
        printf 'use one of: demo, exception, timer, paging, heap, slab, terminal-app\n' >&2
        exit 1
        ;;
esac

"${cmake_bin}" -S "${project_root}" -B "${build_dir}" -G "${generator}" "${cmake_options[@]}"
"${cmake_bin}" --build "${build_dir}" --target kernel
"${create_iso}" "${build_dir}/artifacts/kernel.elf" "${build_dir}/os-lab.iso"
