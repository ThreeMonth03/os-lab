#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    printf 'usage: %s <image.iso>\n' "$0" >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
run_qemu="${script_dir}/../qemu/run_qemu.sh"
image_path=$1

if [[ ! -f "${image_path}" ]]; then
    printf 'image not found: %s\n' "${image_path}" >&2
    exit 1
fi

log_file=$(mktemp)
trap 'rm -f "${log_file}"' EXIT

set +e
timeout 15s "${run_qemu}" "${image_path}" | tee "${log_file}"
status=$?
set -e

if [[ ${status} -ne 0 && ${status} -ne 124 ]]; then
    exit "${status}"
fi

grep -q "os-lab: PIT timer configured at 100 Hz" "${log_file}"
grep -q "os-lab: hardware interrupts enabled" "${log_file}"
grep -q "os-lab: timer smoke waiting for PIT ticks" "${log_file}"
grep -q "os-lab: timer smoke passed ticks=" "${log_file}"
printf 'Timer smoke passed\n'
