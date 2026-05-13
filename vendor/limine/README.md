# Vendored Limine metadata

- Bootloader asset pin: `Limine v12.2.0`
- Binary asset: `https://github.com/Limine-Bootloader/Limine/releases/download/v12.2.0/limine-binary.tar.xz`
- Protocol header source: `https://github.com/Limine-Bootloader/Limine/releases/download/v12.2.0/limine-12.2.0.tar.gz`
- License for `limine.h`: `0BSD`

The build fetches the bootloader binaries on demand into `.cache/limine/`.
The protocol header is committed in-repo so the kernel stays buildable without
pulling extra source trees for includes.
