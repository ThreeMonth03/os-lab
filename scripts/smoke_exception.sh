#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'usage: %s <image.iso> <invalid_opcode|page_fault|divide_error>\n' "$0" >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
image_path=$1
exception_smoke=$2

if [[ ! -f "${image_path}" ]]; then
    printf 'image not found: %s\n' "${image_path}" >&2
    exit 1
fi

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

set +e
timeout 15s "${script_dir}/run_qemu.sh" "${image_path}" | tee "${log_file}"
status=$?
set -e

if [[ ${status} -ne 0 && ${status} -ne 124 ]]; then
    exit "${status}"
fi

grep -q "os-lab: triggering exception smoke: ${expected}" "${log_file}"
grep -q "kernel exception" "${log_file}"
grep -q "name: ${expected}" "${log_file}"
grep -q "vector:" "${log_file}"
grep -q "error code:" "${log_file}"
grep -q "rip:" "${log_file}"
grep -q "rsp:" "${log_file}"
grep -q "rflags:" "${log_file}"
if [[ "${exception_smoke}" == "page_fault" ]]; then
    grep -q "cr2:" "${log_file}"
fi

printf 'Exception smoke passed: %s\n' "${expected}"
