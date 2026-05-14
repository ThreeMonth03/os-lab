#include <stddef.h>
#include <stdint.h>
#include <gtest/gtest.h>
#include "kernel/text/font5x7.hpp"

namespace
{

bool glyph_equals(const kernel::Glyph5x7 & left, const kernel::Glyph5x7 & right)
{
    for (size_t index = 0; index < kernel::Glyph5x7::height; ++index)
    {
        if (left.rows[index] != right.rows[index])
        {
            return false;
        }
    }

    return true;
}

TEST(Font5x7Test, CoversPrintableAsciiWithoutFallback)
{
    for (uint8_t code = kernel::Font5x7::first_printable; code <= kernel::Font5x7::last_printable;
         ++code)
    {
        const char value = static_cast<char>(code);

        EXPECT_TRUE(kernel::Font5x7::has_glyph(value));
        EXPECT_FALSE(
            glyph_equals(kernel::Font5x7::glyph_for(value), kernel::Font5x7::fallback_glyph()))
            << static_cast<int>(code);
    }
}

TEST(Font5x7Test, SpaceGlyphIsBlank)
{
    const kernel::Glyph5x7 & glyph = kernel::Font5x7::glyph_for(' ');

    for (uint8_t row : glyph.rows)
    {
        EXPECT_EQ(row, 0u);
    }
}

TEST(Font5x7Test, UsesFallbackForUnsupportedCharacters)
{
    EXPECT_FALSE(kernel::Font5x7::has_glyph('\n'));
    EXPECT_TRUE(glyph_equals(kernel::Font5x7::glyph_for('\n'), kernel::Font5x7::fallback_glyph()));
}

} // namespace
