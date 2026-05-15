#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 0 ]]; then
    printf 'usage: %s\n' "$0" >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd -- "${script_dir}/../.." && pwd)
build_root=${BUILD_DIR:-"${project_root}/build"}
exception_smoke=${EXCEPTION_SMOKE:-invalid_opcode}

"${script_dir}/smoke_exception.sh" "${build_root}/exception-smoke/${exception_smoke}/os-lab.iso" "${exception_smoke}"
"${script_dir}/smoke_timer.sh" "${build_root}/timer-smoke/os-lab.iso"
"${script_dir}/smoke_paging.sh" "${build_root}/paging-smoke/os-lab.iso"
"${script_dir}/smoke_heap.sh" "${build_root}/heap-smoke/os-lab.iso"
"${script_dir}/smoke_slab.sh" "${build_root}/slab-smoke/os-lab.iso"
