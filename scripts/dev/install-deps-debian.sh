#!/usr/bin/env bash
set -euo pipefail

mode=${1:-host}

case "${mode}" in
    host)
        packages=(
            qemu-system-x86
        )
        ;;
    native)
        packages=(
            clang
            clang-format
            cmake
            curl
            g++
            lld
            make
            mtools
            nasm
            ninja-build
            qemu-system-x86
            xorriso
            xz-utils
        )
        ;;
    *)
        printf 'usage: %s [host|native]\n' "$0" >&2
        exit 1
        ;;
esac

sudo apt-get update
sudo apt-get install -y "${packages[@]}"
