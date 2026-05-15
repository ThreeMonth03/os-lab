#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'usage: %s <image.iso> <invalid_opcode|page_fault|divide_error>\n' "$0" >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source "${script_dir}/lib.sh"

image_path=$1
exception_smoke=$2

smoke_require_iso "${image_path}"

case "${exception_smoke}" in
    invalid_opcode) expected='invalid opcode' ;;
    page_fault) expected='page fault' ;;
    divide_error) expected='divide error' ;;
    *)
        printf 'Invalid EXCEPTION_SMOKE: %s\n' "${exception_smoke}" >&2
        printf 'Use one of: invalid_opcode, page_fault, divide_error\n' >&2
        exit 1
        ;;
esac

log_file=$(mktemp)
trap 'rm -f "${log_file}"' EXIT

smoke_run_qemu_capture "${image_path}" "${log_file}"

smoke_expect "${log_file}" "os-lab: triggering exception smoke: ${expected}"
smoke_expect "${log_file}" "kernel exception"
smoke_expect "${log_file}" "name: ${expected}"
smoke_expect "${log_file}" "vector:"
smoke_expect "${log_file}" "error code:"
smoke_expect "${log_file}" "rip:"
smoke_expect "${log_file}" "rsp:"
smoke_expect "${log_file}" "rflags:"
if [[ "${exception_smoke}" == "page_fault" ]]; then
    smoke_expect "${log_file}" "cr2:"
fi

printf 'Exception smoke passed: %s\n' "${expected}"
