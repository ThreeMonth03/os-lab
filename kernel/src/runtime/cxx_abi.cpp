#include "kernel/core/halt.hpp"

extern "C" int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }

extern "C" void __cxa_pure_virtual() { kernel::halt_forever(); }

void* __dso_handle = nullptr;
