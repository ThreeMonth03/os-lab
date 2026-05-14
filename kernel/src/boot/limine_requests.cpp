#include "kernel/limine_support.hpp"

namespace kernel::boot {

__attribute__((used, section(".limine_requests_start"),
               aligned(8))) volatile uint64_t requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests"), aligned(8))) volatile uint64_t base_revision[] =
    LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests"),
               aligned(8))) volatile limine_bootloader_info_request bootloader_info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used, section(".limine_requests"),
               aligned(8))) volatile limine_firmware_type_request firmware_type_request = {
    .id = LIMINE_FIRMWARE_TYPE_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used, section(".limine_requests"),
               aligned(8))) volatile limine_stack_size_request stack_size_request = {
    .id = LIMINE_STACK_SIZE_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
    .stack_size = 64 * 1024,
};

__attribute__((used, section(".limine_requests"),
               aligned(8))) volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used, section(".limine_requests"),
               aligned(8))) volatile limine_memmap_request memory_map_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = nullptr,
};

__attribute__((used, section(".limine_requests_end"),
               aligned(8))) volatile uint64_t requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

bool base_revision_supported() { return LIMINE_BASE_REVISION_SUPPORTED(base_revision); }

uint64_t loaded_base_revision() {
    if (!LIMINE_LOADED_BASE_REVISION_VALID(base_revision)) {
        return 0;
    }
    return LIMINE_LOADED_BASE_REVISION(base_revision);
}

uint64_t firmware_type() {
    if (firmware_type_request.response == nullptr) {
        return UINT64_MAX;
    }
    return firmware_type_request.response->firmware_type;
}

const limine_bootloader_info_response* bootloader_info() {
    return bootloader_info_request.response;
}

const limine_framebuffer_response* framebuffer() { return framebuffer_request.response; }

const limine_memmap_response* memory_map() { return memory_map_request.response; }

} // namespace kernel::boot
