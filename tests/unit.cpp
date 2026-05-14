#include <stddef.h>

#include <gtest/gtest.h>

#include "kernel/display.hpp"
#include "kernel/fixed_queue.hpp"
#include "kernel/fixed_vector.hpp"
#include "kernel/history.hpp"
#include "kernel/keyboard_decoder.hpp"
#include "kernel/line_editor.hpp"
#include "kernel/mouse_packet_decoder.hpp"
#include "kernel/shell_command.hpp"
#include "kernel/string_view.hpp"
#include "kernel/text_console.hpp"

namespace {

void expect_text(kernel::StringView actual, kernel::StringView expected) {
    EXPECT_EQ(actual.size(), expected.size());
    EXPECT_TRUE(actual == expected);
}

TEST(LineEditorTest, InsertsMovesCursorAndDeletes) {
    kernel::LineEditor line;

    EXPECT_TRUE(line.empty());
    EXPECT_EQ(line.cursor(), 0u);
    EXPECT_FALSE(line.move_left());

    EXPECT_TRUE(line.insert('a'));
    EXPECT_TRUE(line.insert('b'));
    EXPECT_TRUE(line.insert('c'));
    expect_text(line.view(), "abc");
    EXPECT_EQ(line.cursor(), 3u);
    EXPECT_FALSE(line.move_right());

    EXPECT_TRUE(line.move_left());
    EXPECT_TRUE(line.move_left());
    EXPECT_EQ(line.cursor(), 1u);
    EXPECT_TRUE(line.insert('X'));
    expect_text(line.view(), "aXbc");
    EXPECT_EQ(line.cursor(), 2u);

    EXPECT_TRUE(line.backspace());
    expect_text(line.view(), "abc");
    EXPECT_EQ(line.cursor(), 1u);

    EXPECT_TRUE(line.delete_forward());
    expect_text(line.view(), "ac");
    EXPECT_EQ(line.cursor(), 1u);

    EXPECT_TRUE(line.move_to_start());
    EXPECT_EQ(line.cursor(), 0u);
    EXPECT_TRUE(line.delete_forward());
    expect_text(line.view(), "c");
    EXPECT_TRUE(line.move_to_end());
    EXPECT_EQ(line.cursor(), 1u);
    EXPECT_TRUE(line.insert('!'));
    expect_text(line.view(), "c!");

    line.clear();
    EXPECT_TRUE(line.empty());
    EXPECT_EQ(line.cursor(), 0u);
}

TEST(LineEditorTest, ReplacesCurrentLine) {
    kernel::LineEditor line;

    EXPECT_TRUE(line.replace("hello"));
    expect_text(line.view(), "hello");
    EXPECT_EQ(line.cursor(), line.size());

    EXPECT_TRUE(line.move_left());
    EXPECT_TRUE(line.replace("z"));
    expect_text(line.view(), "z");
    EXPECT_EQ(line.cursor(), 1u);

    char too_long[kernel::LineEditor::capacity + 1] = {};
    for (size_t index = 0; index < kernel::LineEditor::capacity + 1; ++index) {
        too_long[index] = 'x';
    }

    EXPECT_FALSE(line.replace(kernel::StringView(too_long, kernel::LineEditor::capacity + 1)));
    expect_text(line.view(), "z");
}

TEST(HistoryTest, BrowsesCommandsAndReturnsBlankAtNewest) {
    kernel::History history;
    kernel::StringView command;

    EXPECT_TRUE(history.empty());
    EXPECT_EQ(history.previous(command), kernel::HistoryResult::None);
    EXPECT_EQ(history.next(command), kernel::HistoryResult::None);
    EXPECT_FALSE(history.push(""));

    EXPECT_TRUE(history.push("one"));
    EXPECT_TRUE(history.push("two"));
    EXPECT_EQ(history.size(), 2u);

    EXPECT_EQ(history.previous(command), kernel::HistoryResult::Command);
    expect_text(command, "two");
    EXPECT_EQ(history.previous(command), kernel::HistoryResult::Command);
    expect_text(command, "one");
    EXPECT_EQ(history.previous(command), kernel::HistoryResult::Command);
    expect_text(command, "one");

    EXPECT_EQ(history.next(command), kernel::HistoryResult::Command);
    expect_text(command, "two");
    EXPECT_EQ(history.next(command), kernel::HistoryResult::Blank);
    EXPECT_TRUE(command.empty());
    EXPECT_EQ(history.next(command), kernel::HistoryResult::None);

    EXPECT_EQ(history.previous(command), kernel::HistoryResult::Command);
    expect_text(command, "two");
    history.reset_browse();
    EXPECT_EQ(history.next(command), kernel::HistoryResult::None);
}

TEST(HistoryTest, KeepsFixedCapacity) {
    kernel::History history;
    kernel::StringView command;

    for (size_t index = 0; index < kernel::History::capacity + 1; ++index) {
        const char value[1] = {static_cast<char>('0' + index)};
        EXPECT_TRUE(history.push({value, 1}));
    }

    EXPECT_EQ(history.size(), kernel::History::capacity);
    EXPECT_EQ(history.previous(command), kernel::HistoryResult::Command);
    expect_text(command, "8");

    for (size_t index = 1; index < kernel::History::capacity; ++index) {
        EXPECT_EQ(history.previous(command), kernel::HistoryResult::Command);
    }
    expect_text(command, "1");
}

TEST(StringViewTest, SupportsBasicViewOperations) {
    const kernel::StringView text = "kernel";

    EXPECT_EQ(text.size(), 6u);
    EXPECT_FALSE(text.empty());
    EXPECT_TRUE(text.starts_with("ker"));
    expect_text(text.substr(1, 3), "ern");
    expect_text(text.substr(3), "nel");
}

TEST(FixedVectorTest, StoresValuesWithoutHeap) {
    kernel::FixedVector<int, 3> values;

    EXPECT_TRUE(values.empty());
    EXPECT_TRUE(values.push_back(1));
    EXPECT_TRUE(values.push_back(2));
    EXPECT_TRUE(values.push_back(3));
    EXPECT_FALSE(values.push_back(4));
    EXPECT_TRUE(values.full());
    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(values[1], 2);
    EXPECT_TRUE(values.pop_back());
    EXPECT_EQ(values.size(), 2u);
    values.clear();
    EXPECT_TRUE(values.empty());
}

TEST(FixedQueueTest, PushesAndPopsInOrder) {
    kernel::FixedQueue<int, 3> values;
    int value = 0;

    EXPECT_TRUE(values.empty());
    EXPECT_TRUE(values.push(1));
    EXPECT_TRUE(values.push(2));
    EXPECT_EQ(values.size(), 2u);

    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_FALSE(values.pop(value));
    EXPECT_TRUE(values.empty());
}

TEST(FixedQueueTest, ReportsFullAndRejectsNewest) {
    kernel::FixedQueue<int, 2> values;
    int value = 0;

    EXPECT_TRUE(values.push(1));
    EXPECT_TRUE(values.push(2));
    EXPECT_TRUE(values.full());
    EXPECT_FALSE(values.push(3));

    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 2);
}

TEST(FixedQueueTest, WrapsAround) {
    kernel::FixedQueue<int, 3> values;
    int value = 0;

    EXPECT_TRUE(values.push(1));
    EXPECT_TRUE(values.push(2));
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(values.push(3));
    EXPECT_TRUE(values.push(4));
    EXPECT_TRUE(values.full());

    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 3);
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 4);
    EXPECT_TRUE(values.empty());
}

TEST(FixedQueueTest, ClearEmptiesQueue) {
    kernel::FixedQueue<int, 2> values;

    EXPECT_TRUE(values.push(1));
    EXPECT_TRUE(values.push(2));
    values.clear();
    EXPECT_TRUE(values.empty());
    EXPECT_FALSE(values.full());
}

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width,
                 uint64_t height) {
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

TEST(DisplayTest, ClipsRectToSurfaceBounds) {
    expect_rect(kernel::display::clip_rect({1, 2, 3, 4}, 10, 10), 1, 2, 3, 4);
    expect_rect(kernel::display::clip_rect({8, 7, 5, 6}, 10, 10), 8, 7, 2, 3);
    expect_rect(kernel::display::clip_rect({10, 0, 1, 1}, 10, 10), 10, 0, 0, 0);
    expect_rect(kernel::display::clip_rect({0, 10, 1, 1}, 10, 10), 0, 10, 0, 0);
}

TEST(DisplayTest, PutPixelAndFillRectAreClipped) {
    uint32_t pixels[12] = {};
    kernel::display::Surface surface(pixels, 4, 3, 4 * sizeof(uint32_t));

    surface.put_pixel(1, 1, {3});
    surface.put_pixel(9, 9, {7});
    EXPECT_EQ(pixels[5], 3u);
    EXPECT_EQ(surface.pixel(1, 1).value, 3u);
    EXPECT_EQ(surface.pixel(9, 9).value, 0u);

    surface.fill_rect({2, 1, 9, 9}, {5});
    EXPECT_EQ(pixels[6], 5u);
    EXPECT_EQ(pixels[7], 5u);
    EXPECT_EQ(pixels[10], 5u);
    EXPECT_EQ(pixels[11], 5u);
    EXPECT_EQ(pixels[0], 0u);
}

TEST(DisplayTest, ScrollUpMovesPixelsAndClearsBottomRows) {
    uint32_t pixels[12] = {
        10, 11, 12, 13, 20, 21, 22, 23, 30, 31, 32, 33,
    };
    kernel::display::Surface surface(pixels, 4, 3, 4 * sizeof(uint32_t));

    surface.scroll_up(1, {0});

    EXPECT_EQ(pixels[0], 20u);
    EXPECT_EQ(pixels[1], 21u);
    EXPECT_EQ(pixels[4], 30u);
    EXPECT_EQ(pixels[5], 31u);
    EXPECT_EQ(pixels[8], 0u);
    EXPECT_EQ(pixels[11], 0u);
}

void expect_cell(kernel::ConsoleCell actual, uint64_t column, uint64_t row) {
    EXPECT_EQ(actual.column, column);
    EXPECT_EQ(actual.row, row);
}

TEST(TextConsoleTest, WritesCharacterAndAdvancesCursor) {
    kernel::TextConsole console(4, 3);

    const kernel::TextConsoleUpdate update = console.write_char('x');

    EXPECT_EQ(update.action, kernel::TextConsoleAction::DrawGlyph);
    expect_cell(update.cell, 0, 0);
    EXPECT_EQ(update.glyph, 'x');
    EXPECT_FALSE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 1u);
    EXPECT_EQ(console.cursor_row(), 0u);
}

TEST(TextConsoleTest, NewlineMovesToNextRow) {
    kernel::TextConsole console(4, 3);
    console.set_cursor(2, 0);

    const kernel::TextConsoleUpdate update = console.newline();

    EXPECT_EQ(update.action, kernel::TextConsoleAction::None);
    EXPECT_FALSE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 1u);
}

TEST(TextConsoleTest, NewlineAtBottomRequestsScroll) {
    kernel::TextConsole console(4, 3);
    console.set_cursor(2, 2);

    const kernel::TextConsoleUpdate update = console.newline();

    EXPECT_TRUE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 2u);
}

TEST(TextConsoleTest, WritingLastBottomCellRequestsScrollAfterDraw) {
    kernel::TextConsole console(4, 3);
    console.set_cursor(3, 2);

    const kernel::TextConsoleUpdate update = console.write_char('z');

    EXPECT_EQ(update.action, kernel::TextConsoleAction::DrawGlyph);
    expect_cell(update.cell, 3, 2);
    EXPECT_EQ(update.glyph, 'z');
    EXPECT_TRUE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 2u);
}

TEST(TextConsoleTest, ClearResetsCursor) {
    kernel::TextConsole console(4, 3);
    console.set_cursor(3, 2);

    console.clear();

    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 0u);
}

TEST(TextConsoleTest, SetCursorClampsToBounds) {
    kernel::TextConsole console(4, 3);

    console.set_cursor(99, 99);

    EXPECT_EQ(console.cursor_column(), 3u);
    EXPECT_EQ(console.cursor_row(), 2u);
}

TEST(TextConsoleTest, BackspaceClearsPreviousCell) {
    kernel::TextConsole console(4, 3);
    console.set_cursor(2, 1);

    const kernel::TextConsoleUpdate update = console.backspace();

    EXPECT_EQ(update.action, kernel::TextConsoleAction::ClearCell);
    expect_cell(update.cell, 1, 1);
    EXPECT_EQ(console.cursor_column(), 1u);
    EXPECT_EQ(console.cursor_row(), 1u);
}

TEST(TextConsoleTest, BackspaceAtLineStartDoesNothing) {
    kernel::TextConsole console(4, 3);
    console.set_cursor(0, 1);

    const kernel::TextConsoleUpdate update = console.backspace();

    EXPECT_EQ(update.action, kernel::TextConsoleAction::None);
    EXPECT_FALSE(update.scroll);
    EXPECT_EQ(console.cursor_column(), 0u);
    EXPECT_EQ(console.cursor_row(), 1u);
}

void expect_command(kernel::StringView input, kernel::ShellCommandKind kind,
                    kernel::StringView text, kernel::StringView name) {
    const kernel::ShellCommand command = kernel::parse_shell_command(input);

    EXPECT_EQ(command.kind, kind);
    expect_text(command.text, text);
    expect_text(command.name, name);
}

TEST(ShellCommandTest, ParsesEmptyAndWhitespaceOnlyInput) {
    expect_command("", kernel::ShellCommandKind::Empty, "", "");
    expect_command("   ", kernel::ShellCommandKind::Empty, "", "");
    expect_command("\t\t", kernel::ShellCommandKind::Empty, "", "");
}

TEST(ShellCommandTest, TrimsKnownCommands) {
    expect_command("help", kernel::ShellCommandKind::Help, "help", "help");
    expect_command("  clear  ", kernel::ShellCommandKind::Clear, "clear", "clear");
    expect_command("\tabout\t", kernel::ShellCommandKind::About, "about", "about");
    expect_command(" halt ", kernel::ShellCommandKind::Halt, "halt", "halt");
}

TEST(ShellCommandTest, ParsesUnknownCommandName) {
    expect_command("wat", kernel::ShellCommandKind::Unknown, "wat", "wat");
    expect_command("  wat  ", kernel::ShellCommandKind::Unknown, "wat", "wat");
    expect_command("wat now", kernel::ShellCommandKind::Unknown, "wat now", "wat");
}

TEST(ShellCommandTest, TreatsKnownCommandsWithArgumentsAsUnknown) {
    expect_command("help now", kernel::ShellCommandKind::Unknown, "help now", "help");
    expect_command("clear now", kernel::ShellCommandKind::Unknown, "clear now", "clear");
    expect_command("about now", kernel::ShellCommandKind::Unknown, "about now", "about");
    expect_command("halt now", kernel::ShellCommandKind::Unknown, "halt now", "halt");
}

kernel::keyboard::KeyEvent expect_key(kernel::keyboard::KeyboardDecoder& decoder,
                                      uint8_t raw_scancode) {
    kernel::keyboard::KeyEvent event;
    EXPECT_TRUE(decoder.decode(raw_scancode, event));
    return event;
}

void expect_no_key(kernel::keyboard::KeyboardDecoder& decoder, uint8_t raw_scancode) {
    kernel::keyboard::KeyEvent event;
    EXPECT_FALSE(decoder.decode(raw_scancode, event));
}

kernel::keyboard::KeyEvent expect_extended_key(kernel::keyboard::KeyboardDecoder& decoder,
                                               uint8_t scancode) {
    expect_no_key(decoder, 0xe0);
    return expect_key(decoder, scancode);
}

TEST(KeyboardDecoderTest, DecodesLowercaseAndShiftUppercase) {
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

TEST(KeyboardDecoderTest, DecodesRightShiftUppercaseAndRelease) {
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

TEST(KeyboardDecoderTest, DecodesCapsLockWithShiftXorBehavior) {
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

TEST(KeyboardDecoderTest, TracksControlAndAltState) {
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

TEST(KeyboardDecoderTest, DecodesExtendedNavigationKeys) {
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

TEST(KeyboardDecoderTest, DoesNotEmitCharactersForKeyRelease) {
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

TEST(MousePacketDecoderTest, DecodesMovementAndButtons) {
    kernel::mouse::MousePacketDecoder decoder;
    kernel::mouse::MousePacket packet;

    EXPECT_FALSE(decoder.decode(0x0b, packet));
    EXPECT_FALSE(decoder.decode(5, packet));
    EXPECT_TRUE(decoder.decode(6, packet));

    EXPECT_EQ(packet.delta_x, 5);
    EXPECT_EQ(packet.delta_y, 6);
    EXPECT_TRUE(packet.left_button);
    EXPECT_TRUE(packet.right_button);
    EXPECT_FALSE(packet.middle_button);
    EXPECT_FALSE(packet.x_overflow);
    EXPECT_FALSE(packet.y_overflow);
}

TEST(MousePacketDecoderTest, DecodesNegativeMovement) {
    kernel::mouse::MousePacketDecoder decoder;
    kernel::mouse::MousePacket packet;

    EXPECT_FALSE(decoder.decode(0x38, packet));
    EXPECT_FALSE(decoder.decode(0xfe, packet));
    EXPECT_TRUE(decoder.decode(0xfd, packet));

    EXPECT_EQ(packet.delta_x, -2);
    EXPECT_EQ(packet.delta_y, -3);
}

TEST(MousePacketDecoderTest, TracksOverflowBits) {
    kernel::mouse::MousePacketDecoder decoder;
    kernel::mouse::MousePacket packet;

    EXPECT_FALSE(decoder.decode(0xc8, packet));
    EXPECT_FALSE(decoder.decode(0, packet));
    EXPECT_TRUE(decoder.decode(0, packet));

    EXPECT_TRUE(packet.x_overflow);
    EXPECT_TRUE(packet.y_overflow);
}

TEST(MousePacketDecoderTest, ResynchronizesOnInvalidFirstByte) {
    kernel::mouse::MousePacketDecoder decoder;
    kernel::mouse::MousePacket packet;

    EXPECT_FALSE(decoder.decode(0x00, packet));
    EXPECT_FALSE(decoder.decode(0x08, packet));
    EXPECT_FALSE(decoder.decode(1, packet));
    EXPECT_TRUE(decoder.decode(2, packet));

    EXPECT_EQ(packet.delta_x, 1);
    EXPECT_EQ(packet.delta_y, 2);
}

} // namespace
