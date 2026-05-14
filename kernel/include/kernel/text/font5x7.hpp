#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kernel {

struct Glyph5x7 {
    static constexpr size_t width = 5;
    static constexpr size_t height = 7;

    uint8_t rows[height];
};

class Font5x7 {
  public:
    static constexpr uint8_t first_printable = 0x20;
    static constexpr uint8_t last_printable = 0x7e;
    static constexpr size_t printable_count = last_printable - first_printable + 1;

    [[nodiscard]] static bool has_glyph(char value);
    [[nodiscard]] static const Glyph5x7& glyph_for(char value);
    [[nodiscard]] static const Glyph5x7& fallback_glyph();
};

} // namespace kernel
