#include "kernel/framebuffer.hpp"

#include <stdint.h>

#include "kernel/limine_support.hpp"

namespace {

uint32_t pack_rgb(const limine_framebuffer& framebuffer, uint8_t red, uint8_t green, uint8_t blue) {
    return (static_cast<uint32_t>(red) << framebuffer.red_mask_shift) |
           (static_cast<uint32_t>(green) << framebuffer.green_mask_shift) |
           (static_cast<uint32_t>(blue) << framebuffer.blue_mask_shift);
}

void fill_rect(limine_framebuffer& framebuffer, uint64_t x, uint64_t y, uint64_t width,
               uint64_t height, uint32_t color) {
    auto* base = static_cast<uint8_t*>(framebuffer.address);

    for (uint64_t row = 0; row < height; ++row) {
        auto* pixel = reinterpret_cast<uint32_t*>(base + ((y + row) * framebuffer.pitch) +
                                                  (x * sizeof(uint32_t)));
        for (uint64_t column = 0; column < width; ++column) {
            pixel[column] = color;
        }
    }
}

} // namespace

namespace kernel::framebuffer {

void paint_boot_splash() {
    const auto* response = boot::framebuffer();
    if (response == nullptr || response->framebuffer_count == 0) {
        return;
    }

    auto* framebuffer = response->framebuffers[0];
    if (framebuffer == nullptr || framebuffer->bpp != 32 ||
        framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB) {
        return;
    }

    const uint32_t background = pack_rgb(*framebuffer, 0x12, 0x17, 0x23);
    const uint32_t accent = pack_rgb(*framebuffer, 0xe0, 0x95, 0x3b);
    const uint32_t highlight = pack_rgb(*framebuffer, 0xf5, 0xf5, 0xf5);

    fill_rect(*framebuffer, 0, 0, framebuffer->width, framebuffer->height, background);
    fill_rect(*framebuffer, 0, 0, framebuffer->width, framebuffer->height / 5, accent);
    fill_rect(*framebuffer, framebuffer->width / 14, framebuffer->height / 3,
              framebuffer->width / 8, framebuffer->height / 8, highlight);
}

} // namespace kernel::framebuffer
