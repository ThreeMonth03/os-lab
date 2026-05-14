# os-lab

An x86_64 kernel playground that starts from zero, uses Limine as the bootloader,
leans on C++ for kernel code, and keeps the day-to-day workflow friendly for both
native Linux and WSL2.

## Current Baseline

- A freestanding C++23 kernel built with `clang++` and `ld.lld`
- A pinned Limine `v12.2.0` fetch flow with checksum verification
- A BIOS/UEFI hybrid ISO image builder
- A host-side QEMU launcher that works on Linux and can bridge to Windows QEMU from WSL2
- A tiny no-heap framebuffer terminal with early PS/2 keyboard input
- GitHub Actions for format, build, Docker sanity, and headless QEMU smoke boot

## Quick Start

Install Docker plus either the Docker Compose plugin or legacy `docker-compose`.
Then install the host QEMU dependency needed to boot the ISO:

```bash
make deps
```

Build the ISO in Docker and boot it with host QEMU:

```bash
make demo
```

`make demo` runs headless and prints the serial log in your terminal. A healthy
boot should include:

```text
os-lab: kernel main entered
os-lab: bootloader = Limine 12.2.0
os-lab: interactive terminal ready
```

To see the QEMU window and framebuffer terminal:

```bash
make gui
```

The early shell supports `help`, `clear`, `about`, and `halt`. The line editor
supports left/right arrows, Backspace, Delete, and the `Ctrl+A`, `Ctrl+C`,
`Ctrl+E`, `Ctrl+L`, and `Ctrl+U` shortcuts. Caps Lock toggles are printed in the
terminal; keyboard LEDs are not managed yet.

In headless mode, quit QEMU with `Ctrl+A`, then `X`.

## Daily Workflow

The recommended flow is to build toolchain-sensitive artifacts in Docker and run
QEMU on the host:

```bash
make demo
```

Useful targets:

```bash
make demo      # Build in Docker, then boot headless
make gui       # Build in Docker, then boot with a QEMU window
make test      # Build in Docker, then run the smoke test
make format    # Apply clang-format inside Docker
make shell     # Open the development container
make clean     # Remove generated files
```

## Native Toolchain

Docker is the default path, but a full native setup is available:

```bash
./scripts/install-deps-debian.sh native
make _iso
make _run
```

## WSL2

- Keep the repo in the Linux filesystem, for example `/home/<you>/os-lab`.
- Install Docker with either `docker compose` or legacy `docker-compose`; the
  Makefile will use whichever one is available.
- Install `QEMU` on Windows and make sure `qemu-system-x86_64.exe` is on the Windows
  `PATH`, or lives in the default `C:\Program Files\qemu\` location.
- `make demo` and `make gui` prefer native Linux QEMU, but on WSL2 they will
  automatically fall back to the Windows executable if needed.

## Layout

```text
.
├── cmake/toolchains/         # clang/lld freestanding toolchain setup
├── config/limine.conf        # boot menu entry
├── kernel/                   # freestanding kernel code and linker script
├── scripts/                  # Limine fetch, ISO creation, QEMU/deps helpers
├── vendor/limine/            # pinned protocol header metadata
└── .github/workflows/ci.yml  # format/build/smoke automation
```

## Next steps

- Add an IDT and structured fault handling
- Bring up a physical memory manager from the Limine memory map
- Expand the early shell and move keyboard input to interrupts
- Add a raw disk image target for USB boot on real hardware
