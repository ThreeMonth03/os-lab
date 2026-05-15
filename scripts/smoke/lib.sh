#!/usr/bin/env bash

smoke_lib_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
smoke_run_qemu="${smoke_lib_dir}/../qemu/run_qemu.sh"

smoke_require_iso() {
    local image_path=$1

    if [[ ! -f "${image_path}" ]]; then
        printf 'image not found: %s\n' "${image_path}" >&2
        exit 1
    fi
}

smoke_run_qemu_capture() {
    local image_path=$1
    local log_file=$2
    local timeout_seconds=${3:-15}
    local status

    set +e
    timeout "${timeout_seconds}s" "${smoke_run_qemu}" "${image_path}" | tee "${log_file}"
    status=${PIPESTATUS[0]}
    set -e

    smoke_accept_qemu_status "${status}"
}

smoke_accept_qemu_status() {
    local status=$1

    if [[ ${status} -ne 0 && ${status} -ne 124 ]]; then
        exit "${status}"
    fi
}

smoke_expect() {
    local log_file=$1
    local expected=$2

    if ! grep -F -q -- "${expected}" "${log_file}"; then
        printf 'missing smoke output: %s\n' "${expected}" >&2
        exit 1
    fi
}
