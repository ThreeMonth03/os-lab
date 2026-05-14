#include "kernel/framebuffer.hpp"

#include <stdint.h>

#include "kernel/display/display.hpp"
#include "kernel/limine_support.hpp"

namespace {

uint32_t pack_rgb(const limine_framebuffer& framebuffer, uint8_t red, uint8_t green, uint8_t blue) {
    return (static_cast<uint32_t>(red) << framebuffer.red_mask_shift) |
           (static_cast<uint32_t>(green) << framebuffer.green_mask_shift) |
           (static_cast<uint32_t>(blue) << framebuffer.blue_mask_shift);
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
    display::Surface surface(framebuffer->address, framebuffer->width, framebuffer->height,
                             framebuffer->pitch);

    surface.fill_rect({0, 0, framebuffer->width, framebuffer->height}, {background});
    surface.fill_rect({0, 0, framebuffer->width, framebuffer->height / 5}, {accent});
    surface.fill_rect({framebuffer->width / 14, framebuffer->height / 3, framebuffer->width / 8,
                       framebuffer->height / 8},
                      {highlight});
}

} // namespace kernel::framebuffer
