#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 0 ]]; then
    printf 'usage: %s\n' "$0" >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_iso="${script_dir}/build_iso.sh"

for kind in exception timer paging heap slab; do
    printf 'Building %s smoke ISO...\n' "${kind}"
    "${build_iso}" "${kind}"
done
