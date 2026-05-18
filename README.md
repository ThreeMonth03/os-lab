# os-lab

An x86_64 kernel playground that starts from zero, uses Limine as the
bootloader, and keeps the day-to-day workflow friendly for both native Linux
and WSL2.

## Current Baseline

- Freestanding C++23 kernel code built with `clang++-19` and `ld.lld-19`
- Pinned Limine `v12.2.0` fetch flow with checksum verification
- BIOS/UEFI hybrid ISO image builder
- Docker-first build workflow with host-side QEMU launch
- WSL2 launcher fallback to Windows QEMU when native Linux QEMU is absent
- Framebuffer terminal, shell line editor, shell history, and static cursor
- PS/2 keyboard and mouse input through IRQ paths
- Software mouse cursor, debug overlay, compositor dirty-region foundation, and
  opt-in minimal GUI panel
- Early memory map parsing, frame allocator, paging, heap, and slab foundations
- Host-side GTest, clang-format, clang-tidy, and QEMU smoke paths

## Quick Start

Install Docker plus either the Docker Compose plugin or `docker-compose`.
Then install the host QEMU dependency needed to boot the ISO:

```bash
make deps
```

Build the ISO in Docker and boot it headless:

```bash
make demo
```

`make demo` prints the serial log in your terminal. A healthy boot should
include:

```text
os-lab: kernel main entered
os-lab: framebuffer terminal active
os-lab: interactive terminal ready
```

To see the QEMU window and framebuffer terminal:

```bash
make gui
```

In headless mode, quit QEMU with `Ctrl+A`, then `X`.

## Shell

The early shell currently supports:

```text
help
clear
about
halt
mem
input
heap
slab
```

The line editor supports printable ASCII, Tab expansion, left/right arrows,
Backspace, Delete, history Up/Down, and `Ctrl+A`, `Ctrl+C`, `Ctrl+E`,
`Ctrl+L`, and `Ctrl+U`. Caps Lock changes the prompt indicator; keyboard LEDs
are not managed yet.

## Common Targets

```bash
make demo
make gui
make gui GUI_PANEL_VISIBLE=ON
make gui TERMINAL_WINDOW_CHROME=ON
make gui TERMINAL_WINDOW_CHROME=ON TERMINAL_WINDOW_INTERACTION=ON
make profile-gui
make test
make unit
make format
make tidy
make test-exception EXCEPTION_SMOKE=page_fault
make test-timer
make test-paging
make test-heap
make test-slab
make test-smoke
make clean
```

`EXCEPTION_SMOKE` accepts `invalid_opcode`, `page_fault`, or `divide_error`.
Normal `make demo` and `make gui` always build with exception triggers disabled.

`make gui` is the stable default path. `TERMINAL_WINDOW_CHROME=ON` and
`TERMINAL_WINDOW_INTERACTION=ON` are debug-only terminal app/window experiments
for the future desktop path. `GUI_PANEL_VISIBLE=ON` is a legacy transitional
panel flag; it remains available for comparison but should not be a dependency
for new window interaction work.

## Native Toolchain

Docker is the default path, but a full native setup is available:

```bash
./scripts/dev/install-deps-debian.sh native
make _iso
make _run
```

## WSL2

- Keep the repo in the Linux filesystem, for example `/home/<you>/os-lab`.
- Install Docker with either `docker compose` or `docker-compose`; the Makefile
  will use whichever one is available.
- Install QEMU on Windows and make sure `qemu-system-x86_64.exe` is on the
  Windows `PATH`, or lives in `C:\Program Files\qemu\`.
- `make demo` and `make gui` prefer native Linux QEMU, but on WSL2 they fall
  back to the Windows executable if needed.
- Mouse input currently uses a PS/2 relative mouse. When the QEMU GUI first
  opens, the host cursor and guest software cursor can start out unsynchronized;
  moving the mouse a little should settle it. A future absolute pointer path
  such as USB tablet or virtio input would improve this.

## Layout

```text
.
├── cmake/toolchains/         # clang/lld freestanding toolchain setup
├── config/limine.conf        # boot menu entry
├── kernel/                   # freestanding kernel code and linker script
├── scripts/                  # build/dev/qemu/smoke helper scripts
├── tests/                    # host-side GTest coverage
├── vendor/limine/            # pinned protocol header metadata
└── .github/workflows/ci.yml  # format/build/smoke automation
```

More project policy lives in [STYLE.md](STYLE.md). Current direction lives in
[PLAN.md](PLAN.md).
