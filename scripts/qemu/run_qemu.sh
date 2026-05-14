#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
    printf 'usage: %s <image.iso> [extra qemu args...]\n' "$0" >&2
    exit 1
fi

image_path=$1
shift

if [[ ! -f "${image_path}" ]]; then
    printf 'image not found: %s\n' "${image_path}" >&2
    exit 1
fi

find_windows_qemu() {
    local candidate windows_path

    if windows_path=$(cmd.exe /C where qemu-system-x86_64.exe 2>/dev/null | tr -d '\r' | head -n 1); then
        if [[ -n "${windows_path}" ]]; then
            wslpath -u "${windows_path}"
            return 0
        fi
    fi

    for candidate in \
        "/mnt/c/Program Files/qemu/qemu-system-x86_64.exe" \
        "/mnt/c/Program Files (x86)/qemu/qemu-system-x86_64.exe"; do
        if [[ -x "${candidate}" ]]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done

    return 1
}

if command -v qemu-system-x86_64 >/dev/null 2>&1; then
    qemu_bin=$(command -v qemu-system-x86_64)
    image_arg=${image_path}
elif [[ -n "${WSL_DISTRO_NAME:-}" ]]; then
    qemu_bin=$(find_windows_qemu || true)
    if [[ -z "${qemu_bin:-}" ]]; then
        printf 'QEMU not found. Install qemu-system-x86 on Linux or Windows QEMU for WSL2.\n' >&2
        exit 1
    fi
    image_arg=$(wslpath -w "${image_path}")
else
    printf 'QEMU not found in PATH.\n' >&2
    exit 1
fi

qemu_args=(
    -M q35
    -m "${QEMU_MEMORY:-256M}"
    -boot d
    -cdrom "${image_arg}"
    -serial stdio
    -no-reboot
    -no-shutdown
)

if [[ "${QEMU_HEADLESS:-1}" == "1" ]]; then
    qemu_args+=(-display none)
fi

exec "${qemu_bin}" "${qemu_args[@]}" "$@"
