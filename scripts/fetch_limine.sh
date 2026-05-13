#!/usr/bin/env bash
set -euo pipefail

limine_version=${LIMINE_VERSION:-12.2.0}
limine_url=${LIMINE_URL:-"https://github.com/Limine-Bootloader/Limine/releases/download/v${limine_version}/limine-binary.tar.xz"}
limine_sha256=${LIMINE_SHA256:-55100c376fbda1badb9d3208cca2a4df2dabacf484af84f7356cbd259fe0b29a}

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd -- "${script_dir}/.." && pwd)
cache_root="${project_root}/.cache/limine/${limine_version}"
archive_path="${cache_root}/limine-binary.tar.xz"
extract_root="${cache_root}/limine-binary"

mkdir -p "${cache_root}"

if [[ ! -f "${archive_path}" ]]; then
    curl -fsSL "${limine_url}" -o "${archive_path}"
fi

printf '%s  %s\n' "${limine_sha256}" "${archive_path}" | sha256sum -c >/dev/null

if [[ ! -d "${extract_root}" ]]; then
    tar -xf "${archive_path}" -C "${cache_root}"
fi

printf '%s\n' "${extract_root}"
