#include "kernel/display.hpp"

namespace kernel::display {

Surface::Surface(void* address, uint64_t width, uint64_t height, uint64_t pitch)
    : address_(address), width_(width), height_(height), pitch_(pitch) {}

Color Surface::pixel(uint64_t x, uint64_t y) const {
    if (!ready() || x >= width_ || y >= height_) {
        return {};
    }

    const auto* base = static_cast<const uint8_t*>(address_);
    const auto* pixel =
        reinterpret_cast<const uint32_t*>(base + (y * pitch_) + (x * sizeof(uint32_t)));
    return {*pixel};
}

void Surface::put_pixel(uint64_t x, uint64_t y, Color color) {
    if (!ready() || x >= width_ || y >= height_) {
        return;
    }

    auto* base = static_cast<uint8_t*>(address_);
    auto* pixel = reinterpret_cast<uint32_t*>(base + (y * pitch_) + (x * sizeof(uint32_t)));
    *pixel = color.value;
}

void Surface::fill_rect(Rect rect, Color color) {
    rect = clip_rect(rect, width_, height_);
    if (!ready() || rect.empty()) {
        return;
    }

    for (uint64_t row = 0; row < rect.height; ++row) {
        auto* base = static_cast<uint8_t*>(address_);
        auto* pixel = reinterpret_cast<uint32_t*>(base + ((rect.y + row) * pitch_) +
                                                  (rect.x * sizeof(uint32_t)));
        for (uint64_t column = 0; column < rect.width; ++column) {
            pixel[column] = color.value;
        }
    }
}

void Surface::scroll_up(uint64_t pixel_count, Color clear_color) {
    if (!ready()) {
        return;
    }

    if (pixel_count >= height_) {
        fill_rect({0, 0, width_, height_}, clear_color);
        return;
    }

    auto* base = static_cast<uint8_t*>(address_);
    const uint64_t copy_height = height_ - pixel_count;

    for (uint64_t y = 0; y < copy_height; ++y) {
        auto* destination = base + (y * pitch_);
        const auto* source = base + ((y + pixel_count) * pitch_);
        for (uint64_t byte = 0; byte < pitch_; ++byte) {
            destination[byte] = source[byte];
        }
    }

    fill_rect({0, copy_height, width_, pixel_count}, clear_color);
}

Rect clip_rect(Rect rect, uint64_t width, uint64_t height) {
    if (rect.x >= width || rect.y >= height) {
        return {rect.x, rect.y, 0, 0};
    }

    const uint64_t available_width = width - rect.x;
    const uint64_t available_height = height - rect.y;
    if (rect.width > available_width) {
        rect.width = available_width;
    }
    if (rect.height > available_height) {
        rect.height = available_height;
    }

    return rect;
}

} // namespace kernel::display
