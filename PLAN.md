# Plan

This file tracks near-term direction. It is intentionally shorter than a full
design document.

## Current Foundation

- Boot and ISO build through Limine
- Freestanding C++23 kernel target
- Framebuffer terminal and early shell
- PS/2 keyboard and mouse IRQ input
- Display primitives, compositor dirty regions, debug overlay, and opt-in GUI
  panel
- IDT, exception handling, PIC IRQ, PIT timer, active paging, heap, and slab
  foundations
- Host-side unit tests plus QEMU smoke paths

## Cleanup Track

- Finish namespace alignment for text, shell, pointer, keyboard, and mouse
  boundaries.
- Keep architecture assembly stubs in `.S` files rather than large inline asm
  strings inside C++.
- Keep README focused on workflow and keep policy in `STYLE.md`.
- Remove compatibility wrappers and unused helpers when `rg` shows no current
  caller.

## Feature Track

- Improve display/input boundaries before adding real widgets.
- Add debug-only focus switching only after hit testing and routing remain
  stable.
- Defer filesystem, scheduler, userspace, and full window management until the
  memory/input/display foundations are cleaner.

## Manual Checks

`make gui` is still useful for things automation cannot fully see:

- keyboard input, Caps Lock prompt indicator, history, and Ctrl shortcuts
- mouse movement and cursor layering
- scrolling terminal output with debug overlay and optional GUI panel
- `GUI_PANEL_VISIBLE=ON` panel drawing and lack of cursor artifacts
