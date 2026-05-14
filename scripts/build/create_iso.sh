#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
    printf 'usage: %s <kernel-elf> <output-iso>\n' "$0" >&2
    exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
project_root=$(cd -- "${script_dir}/../.." && pwd)
kernel_elf=$1
output_iso=$2
iso_root="${project_root}/build/iso-root"

if [[ ! -f "${kernel_elf}" ]]; then
    printf 'kernel ELF not found: %s\n' "${kernel_elf}" >&2
    exit 1
fi

limine_dir=$("${script_dir}/fetch_limine.sh")

if [[ ! -x "${limine_dir}/limine" ]]; then
    make -C "${limine_dir}" CC="${HOST_CC:-clang}" >/dev/null
fi

rm -rf "${iso_root}"
mkdir -p "${iso_root}/boot/limine" "${iso_root}/EFI/BOOT" "$(dirname -- "${output_iso}")"

cp "${kernel_elf}" "${iso_root}/boot/kernel.elf"
cp "${project_root}/config/limine.conf" "${iso_root}/boot/limine/limine.conf"
cp "${limine_dir}/limine-bios.sys" "${iso_root}/boot/limine/"
cp "${limine_dir}/limine-bios-cd.bin" "${iso_root}/boot/limine/"
cp "${limine_dir}/limine-uefi-cd.bin" "${iso_root}/boot/limine/"
cp "${limine_dir}/BOOTX64.EFI" "${iso_root}/EFI/BOOT/"
cp "${limine_dir}/BOOTIA32.EFI" "${iso_root}/EFI/BOOT/"

rm -f "${output_iso}"

xorriso -as mkisofs \
    -R -r -J \
    -b boot/limine/limine-bios-cd.bin \
    -no-emul-boot \
    -boot-load-size 4 \
    -boot-info-table \
    -eltorito-alt-boot \
    -e boot/limine/limine-uefi-cd.bin \
    -no-emul-boot \
    -isohybrid-gpt-basdat \
    "${iso_root}" \
    -o "${output_iso}" >/dev/null

"${limine_dir}/limine" bios-install "${output_iso}" >/dev/null

printf 'ISO ready: %s\n' "${output_iso}"
