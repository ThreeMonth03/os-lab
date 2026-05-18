#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    printf 'usage: %s <image.iso>\n' "$0" >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "${script_dir}/lib.sh"

image_path=$1

smoke_require_iso "${image_path}"

log_file=$(mktemp)
trap 'rm -f "${log_file}"' EXIT

smoke_run_qemu_capture "${image_path}" "${log_file}"

smoke_expect "${log_file}" "os-lab: terminal app lifecycle smoke started"
smoke_expect "${log_file}" "os-lab: terminal app lifecycle smoke passed"
printf 'Terminal app lifecycle smoke passed\n'
