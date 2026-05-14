#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'usage: %s <apply|check> <clang-format>\n' "$0" >&2
    exit 1
fi

mode=$1
clang_format=$2
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd -- "${script_dir}/../.." && pwd)

mapfile -d '' sources < <(
    find \
        "${project_root}/kernel/include/kernel" \
        "${project_root}/kernel/src" \
        "${project_root}/tests" \
        -type f \
        \( -name '*.hpp' -o -name '*.cpp' \) \
        -print0 | sort -z
)

if [[ ${#sources[@]} -eq 0 ]]; then
    exit 0
fi

case "${mode}" in
    apply)
        "${clang_format}" -i "${sources[@]}"
        ;;
    check)
        "${clang_format}" --dry-run --Werror "${sources[@]}"
        ;;
    *)
        printf 'invalid format mode: %s\n' "${mode}" >&2
        printf 'use one of: apply, check\n' >&2
        exit 1
        ;;
esac
