#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    printf 'usage: %s <image.iso>\n' "$0" >&2
    exit 1
fi

image_path=$1

if [[ ! -f "${image_path}" ]]; then
    printf 'image not found: %s\n' "${image_path}" >&2
    exit 1
fi

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    printf 'Smoke test requires native Linux qemu-system-x86_64 in PATH\n' >&2
    exit 1
fi

log_file=$(mktemp)
debug_file=$(mktemp)
trap 'rm -f "${log_file}" "${debug_file}"' EXIT

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

if [[ ${status} -ne 0 && ${status} -ne 124 ]]; then
    exit "${status}"
fi

cat "${debug_file}" >> "${log_file}"
grep -q "os-lab: reached _start" "${log_file}"
grep -q "os-lab: entering kernel_main" "${log_file}"
printf 'Smoke test passed\n'
