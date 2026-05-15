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
trap 'rm -f "${log_file}" "${debug_file:-}"' EXIT

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    printf 'Native QEMU not found; running serial-only smoke through QEMU launcher.\n'
    smoke_run_qemu_capture "${image_path}" "${log_file}"
    smoke_expect "${log_file}" "os-lab: kernel main entered"
    smoke_expect "${log_file}" "os-lab: framebuffer terminal active"
    printf 'Smoke test passed\n'
    exit 0
fi

debug_file=$(mktemp)

set +e
timeout 15s qemu-system-x86_64 \
    -M q35 \
    -m 256M \
    -boot d \
    -cdrom "${image_path}" \
    -serial stdio \
    -debugcon "file:${debug_file}" \
    -display none \
    -no-reboot \
    -no-shutdown | tee "${log_file}"
status=$?
set -e

smoke_accept_qemu_status "${status}"

cat "${debug_file}" >> "${log_file}"
smoke_expect "${log_file}" "os-lab: reached _start"
smoke_expect "${log_file}" "os-lab: entering kernel_main"
printf 'Smoke test passed\n'
