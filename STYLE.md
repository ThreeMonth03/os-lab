# Style

This project is a freestanding kernel, so the first rule is to keep boundaries
explicit. Code may use modern C++23, but it must not quietly become hosted C++.

## Kernel C++ Boundary

- Do not introduce hosted C++ standard library dependencies into kernel
  targets.
- Do not use heap-backed designs unless the subsystem explicitly owns that
  allocation boundary.
- Prefer small value types, fixed-capacity containers, and caller-provided
  storage.
- Keep global mutable state inside the translation unit that owns the
  subsystem.
- Avoid C-style casts; use `static_cast` or `reinterpret_cast` so intent is
  visible.
- POD, ABI, hardware descriptor, and bootloader-facing data may stay as
  `struct`.

## Namespace And Layout

Namespaces should generally follow the include/source ownership:

- `kernel::arch::x86_64` for CPU and x86_64 platform code
- `kernel::drivers` for hardware-facing drivers
- `kernel::display` for framebuffer primitives, compositor, overlay, panel, and
  render helpers
- `kernel::console` for terminal facade code
- `kernel::input` for device-independent input queues, routing, stats, and
  pointer state
- `kernel::memory` for memory map, frame allocator, heap, and slab code
- `kernel::shell` for shell parsing, command execution, and shell orchestration
- `kernel::text` for pure text editing, history, font, and console layout

Keep low-level base utilities such as `StringView`, `Span`, `FixedVector`, and
`FixedQueue` in the root `kernel` namespace even though their headers live under
`kernel/base/`.

## `[[nodiscard]]`

Use `[[nodiscard]]` where ignoring a result is likely to hide a real bug:

- initialization, registration, create, allocate, map, unmap, free, validate
- parse, decode, pop, find, lookup, translate

Avoid `[[nodiscard]]` on plain getters, best-effort drawing helpers, dirty
marking helpers, and APIs where ignoring the result is a normal call-site
choice.

## Class And Struct Closing Comments

Use closing comments where they help scanning without adding churn:

- Non-trivial public header classes may end with `}; // end class Name`.
- Larger semantic public structs may end with `}; // end struct Name`.
- Short POD snapshots, enum classes, ABI descriptors, and hardware descriptors
  do not need closing comments.
- Namespace closing comments should stay in the existing `} // namespace ...`
  form.

## Assembly Boundary

Prefer `.S` files for architecture stubs that define global ABI symbols, such
as interrupt and exception entry points. C++ files should keep the typed setup,
dispatch, and declarations:

```cpp
extern "C" void kernel_x86_64_exception_0();
extern "C" void kernel_x86_64_exception_dispatch(...);
```

Inline assembly inside C++ is still reasonable for tiny CPU instructions such
as `lidt`, `cli`, `sti`, `rdmsr`, `wrmsr`, and control register reads.
