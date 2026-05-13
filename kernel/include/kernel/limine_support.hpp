#pragma once

#include <stdint.h>

#include <limine.h>

namespace kernel::boot {

bool base_revision_supported();
uint64_t loaded_base_revision();
uint64_t firmware_type();
const limine_bootloader_info_response* bootloader_info();
const limine_framebuffer_response* framebuffer();

} // namespace kernel::boot
