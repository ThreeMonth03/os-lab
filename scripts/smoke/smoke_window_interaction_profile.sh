#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'usage: %s <image.iso> <log-file>\n' "$0" >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "${script_dir}/lib.sh"

image_path=$1
log_file=$2

smoke_require_iso "${image_path}"
mkdir -p "$(dirname -- "${log_file}")"

smoke_run_qemu_capture "${image_path}" "${log_file}" 25
smoke_expect "${log_file}" "os-lab: window interaction profile running"
smoke_expect "${log_file}" "os-lab display profile: command=window-drag-dummy:drag-start"
smoke_expect "${log_file}" "os-lab display profile: command=window-drag-dummy:drag-steps"
smoke_expect "${log_file}" "os-lab display profile: command=window-drag-dummy:drag-release"
smoke_expect "${log_file}" "os-lab display profile: command=window-drag-terminal:drag-start"
smoke_expect "${log_file}" "os-lab display profile: command=window-drag-terminal:drag-steps"
smoke_expect "${log_file}" "os-lab display profile: command=window-drag-terminal:drag-release"
smoke_expect "${log_file}" "os-lab display profile: command=window-resize-dummy:resize-start"
smoke_expect "${log_file}" "os-lab display profile: command=window-resize-dummy:resize-steps"
smoke_expect "${log_file}" "os-lab display profile: command=window-resize-dummy:resize-release"
smoke_expect "${log_file}" "os-lab display profile: command=window-resize-terminal:resize-start"
smoke_expect "${log_file}" "os-lab display profile: command=window-resize-terminal:resize-steps"
smoke_expect "${log_file}" "os-lab display profile: command=window-resize-terminal:resize-release"
smoke_expect "${log_file}" "os-lab display profile: command=window-bar-reopen-terminal:hide-click"
smoke_expect "${log_file}" "os-lab display profile: command=window-bar-reopen-terminal:bar-click"
smoke_expect "${log_file}" "os-lab display profile: command=window-focus-switch:dummy-title-click"
smoke_expect "${log_file}" "os-lab display profile: command=window-focus-switch:terminal-title-click"
smoke_expect "${log_file}" "os-lab: window interaction profile done"

printf 'Window interaction profile log: %s\n' "${log_file}"
