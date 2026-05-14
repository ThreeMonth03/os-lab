#include <stdint.h>
#include <gtest/gtest.h>
#include "kernel/input/keyboard_decoder.hpp"

namespace
{

kernel::keyboard::KeyEvent expect_key(kernel::keyboard::KeyboardDecoder & decoder,
                                      uint8_t raw_scancode)
{
    kernel::keyboard::KeyEvent event;
    EXPECT_TRUE(decoder.decode(raw_scancode, event));
    return event;
}

void expect_no_key(kernel::keyboard::KeyboardDecoder & decoder, uint8_t raw_scancode)
{
    kernel::keyboard::KeyEvent event;
    EXPECT_FALSE(decoder.decode(raw_scancode, event));
}

kernel::keyboard::KeyEvent expect_extended_key(kernel::keyboard::KeyboardDecoder & decoder,
                                               uint8_t scancode)
{
    expect_no_key(decoder, 0xe0);
    return expect_key(decoder, scancode);
}

void expect_character_key(kernel::keyboard::KeyboardDecoder & decoder, uint8_t scancode, char expected)
{
    const kernel::keyboard::KeyEvent event = expect_key(decoder, scancode);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Character);
    EXPECT_EQ(event.character, expected);
    EXPECT_TRUE(event.pressed);
}

TEST(KeyboardDecoderTest, DecodesLowercaseAndShiftUppercase)
{
    kernel::keyboard::KeyboardDecoder decoder;

    kernel::keyboard::KeyEvent event = expect_key(decoder, 0x1e);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Character);
    EXPECT_EQ(event.character, 'a');
    EXPECT_TRUE(event.pressed);
    EXPECT_FALSE(event.shift);
    EXPECT_FALSE(event.caps_lock);

    event = expect_key(decoder, 0x2a);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Shift);
    EXPECT_TRUE(event.shift);
    EXPECT_TRUE(event.pressed);

    event = expect_key(decoder, 0x1e);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Character);
    EXPECT_EQ(event.character, 'A');
    EXPECT_TRUE(event.shift);

    event = expect_key(decoder, 0xaa);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Shift);
    EXPECT_FALSE(event.pressed);
    EXPECT_FALSE(event.shift);
}

TEST(KeyboardDecoderTest, DecodesRightShiftUppercaseAndRelease)
{
    kernel::keyboard::KeyboardDecoder decoder;

    kernel::keyboard::KeyEvent event = expect_key(decoder, 0x36);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Shift);
    EXPECT_TRUE(event.shift);
    EXPECT_TRUE(event.pressed);

    event = expect_key(decoder, 0x1e);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Character);
    EXPECT_EQ(event.character, 'A');
    EXPECT_TRUE(event.shift);

    event = expect_key(decoder, 0xb6);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Shift);
    EXPECT_FALSE(event.pressed);
    EXPECT_FALSE(event.shift);

    event = expect_key(decoder, 0x1e);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Character);
    EXPECT_EQ(event.character, 'a');
    EXPECT_FALSE(event.shift);
}

TEST(KeyboardDecoderTest, DecodesCapsLockWithShiftXorBehavior)
{
    kernel::keyboard::KeyboardDecoder decoder;

    kernel::keyboard::KeyEvent event = expect_key(decoder, 0x3a);
    EXPECT_EQ(event.key, kernel::keyboard::Key::CapsLock);
    EXPECT_TRUE(event.caps_lock);

    event = expect_key(decoder, 0x1e);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Character);
    EXPECT_EQ(event.character, 'A');
    EXPECT_FALSE(event.shift);
    EXPECT_TRUE(event.caps_lock);

    event = expect_key(decoder, 0x2a);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Shift);
    EXPECT_TRUE(event.shift);
    EXPECT_TRUE(event.caps_lock);

    event = expect_key(decoder, 0x1e);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Character);
    EXPECT_EQ(event.character, 'a');
    EXPECT_TRUE(event.shift);
    EXPECT_TRUE(event.caps_lock);
}

TEST(KeyboardDecoderTest, TracksControlAndAltState)
{
    kernel::keyboard::KeyboardDecoder decoder;

    kernel::keyboard::KeyEvent event = expect_key(decoder, 0x1d);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Control);
    EXPECT_TRUE(event.control);
    EXPECT_TRUE(event.pressed);

    event = expect_key(decoder, 0x1e);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Character);
    EXPECT_EQ(event.character, 'a');
    EXPECT_TRUE(event.control);

    event = expect_key(decoder, 0x9d);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Control);
    EXPECT_FALSE(event.control);
    EXPECT_FALSE(event.pressed);

    event = expect_key(decoder, 0x38);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Alt);
    EXPECT_TRUE(event.alt);
    EXPECT_TRUE(event.pressed);
    expect_no_key(decoder, 0x1e);

    event = expect_key(decoder, 0xb8);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Alt);
    EXPECT_FALSE(event.alt);
    EXPECT_FALSE(event.pressed);
}

TEST(KeyboardDecoderTest, DecodesExtendedNavigationKeys)
{
    kernel::keyboard::KeyboardDecoder decoder;

    kernel::keyboard::KeyEvent event = expect_extended_key(decoder, 0x4b);
    EXPECT_EQ(event.key, kernel::keyboard::Key::LeftArrow);
    EXPECT_TRUE(event.extended);
    EXPECT_TRUE(event.pressed);

    event = expect_extended_key(decoder, 0x4d);
    EXPECT_EQ(event.key, kernel::keyboard::Key::RightArrow);
    EXPECT_TRUE(event.extended);

    event = expect_extended_key(decoder, 0x48);
    EXPECT_EQ(event.key, kernel::keyboard::Key::UpArrow);
    EXPECT_TRUE(event.extended);

    event = expect_extended_key(decoder, 0x50);
    EXPECT_EQ(event.key, kernel::keyboard::Key::DownArrow);
    EXPECT_TRUE(event.extended);

    event = expect_extended_key(decoder, 0x53);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Delete);
    EXPECT_TRUE(event.extended);
}

TEST(KeyboardDecoderTest, DoesNotEmitCharactersForKeyRelease)
{
    kernel::keyboard::KeyboardDecoder decoder;

    expect_no_key(decoder, 0x9e);

    kernel::keyboard::KeyEvent event = expect_key(decoder, 0x2a);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Shift);
    EXPECT_TRUE(event.pressed);

    event = expect_key(decoder, 0xaa);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Shift);
    EXPECT_FALSE(event.pressed);
    EXPECT_EQ(event.character, '\0');
}

TEST(KeyboardDecoderTest, DecodesTabPressOnly)
{
    kernel::keyboard::KeyboardDecoder decoder;

    kernel::keyboard::KeyEvent event = expect_key(decoder, 0x0f);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Tab);
    EXPECT_TRUE(event.pressed);
    EXPECT_EQ(event.character, '\0');

    expect_no_key(decoder, 0x8f);
}

TEST(KeyboardDecoderTest, DecodesPunctuationAndShiftedPunctuation)
{
    kernel::keyboard::KeyboardDecoder decoder;

    expect_character_key(decoder, 0x2b, '\\');
    expect_character_key(decoder, 0x29, '`');
    expect_character_key(decoder, 0x1a, '[');
    expect_character_key(decoder, 0x1b, ']');
    expect_character_key(decoder, 0x27, ';');
    expect_character_key(decoder, 0x28, '\'');

    kernel::keyboard::KeyEvent event = expect_key(decoder, 0x2a);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Shift);
    EXPECT_TRUE(event.shift);

    expect_character_key(decoder, 0x2b, '|');
    expect_character_key(decoder, 0x29, '~');
    expect_character_key(decoder, 0x1a, '{');
    expect_character_key(decoder, 0x1b, '}');
    expect_character_key(decoder, 0x27, ':');
    expect_character_key(decoder, 0x28, '"');
    expect_character_key(decoder, 0x02, '!');
    expect_character_key(decoder, 0x03, '@');
    expect_character_key(decoder, 0x04, '#');
    expect_character_key(decoder, 0x05, '$');
    expect_character_key(decoder, 0x06, '%');
    expect_character_key(decoder, 0x07, '^');
    expect_character_key(decoder, 0x08, '&');
    expect_character_key(decoder, 0x09, '*');
}

} // namespace
