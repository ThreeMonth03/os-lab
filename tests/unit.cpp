#include <stddef.h>

#include <gtest/gtest.h>

#include "kernel/display.hpp"
#include "kernel/editor_dirty_range.hpp"
#include "kernel/editor_view_layout.hpp"
#include "kernel/fixed_queue.hpp"
#include "kernel/fixed_vector.hpp"
#include "kernel/font5x7.hpp"
#include "kernel/history.hpp"
#include "kernel/keyboard_decoder.hpp"
#include "kernel/line_editor.hpp"
#include "kernel/memory_map.hpp"
#include "kernel/mouse_packet_decoder.hpp"
#include "kernel/physical_frame_allocator.hpp"
#include "kernel/pointer_state.hpp"
#include "kernel/shell_command.hpp"
#include "kernel/string_view.hpp"
#include "kernel/text_console.hpp"

namespace {

void expect_text(kernel::StringView actual, kernel::StringView expected) {
    EXPECT_EQ(actual.size(), expected.size());
    EXPECT_TRUE(actual == expected);
}

bool glyph_equals(const kernel::Glyph5x7& left, const kernel::Glyph5x7& right) {
    for (size_t index = 0; index < kernel::Glyph5x7::height; ++index) {
        if (left.rows[index] != right.rows[index]) {
            return false;
        }
    }

    return true;
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

TEST(LineEditorTest, ComputesTabStopSpacing) {
    EXPECT_EQ(kernel::LineEditor::spaces_to_next_tab_stop(0), 4u);
    EXPECT_EQ(kernel::LineEditor::spaces_to_next_tab_stop(1), 3u);
    EXPECT_EQ(kernel::LineEditor::spaces_to_next_tab_stop(3), 1u);
    EXPECT_EQ(kernel::LineEditor::spaces_to_next_tab_stop(4), 4u);
}

TEST(LineEditorTest, InsertsSpacesAtCursor) {
    kernel::LineEditor line;

    EXPECT_TRUE(line.insert('a'));
    EXPECT_TRUE(line.insert('b'));
    EXPECT_TRUE(line.move_left());
    EXPECT_TRUE(line.insert_spaces(kernel::LineEditor::spaces_to_next_tab_stop(line.cursor())));

    expect_text(line.view(), "a   b");
    EXPECT_EQ(line.cursor(), 4u);
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

TEST(HistoryTest, StoresFullLineEditorCapacityCommands) {
    kernel::History history;
    kernel::StringView command;
    char text[kernel::LineEditor::capacity] = {};

    for (size_t index = 0; index < kernel::LineEditor::capacity; ++index) {
        text[index] = 'x';
    }

    EXPECT_TRUE(history.push({text, kernel::LineEditor::capacity}));
    EXPECT_EQ(history.previous(command), kernel::HistoryResult::Command);
    EXPECT_EQ(command.size(), kernel::LineEditor::capacity);
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

    EXPECT_EQ(values.available(), 2u);
    EXPECT_TRUE(values.push(1));
    EXPECT_EQ(values.available(), 1u);
    EXPECT_TRUE(values.push(2));
    EXPECT_TRUE(values.full());
    EXPECT_EQ(values.available(), 0u);
    EXPECT_FALSE(values.push(3));

    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_EQ(values.available(), 1u);
    EXPECT_TRUE(values.pop(value));
    EXPECT_EQ(value, 2);
    EXPECT_EQ(values.available(), 2u);
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
    EXPECT_EQ(values.available(), values.capacity());
}

TEST(Font5x7Test, CoversPrintableAsciiWithoutFallback) {
    for (uint8_t code = kernel::Font5x7::first_printable; code <= kernel::Font5x7::last_printable;
         ++code) {
        const char value = static_cast<char>(code);

        EXPECT_TRUE(kernel::Font5x7::has_glyph(value));
        EXPECT_FALSE(
            glyph_equals(kernel::Font5x7::glyph_for(value), kernel::Font5x7::fallback_glyph()))
            << static_cast<int>(code);
    }
}

TEST(Font5x7Test, SpaceGlyphIsBlank) {
    const kernel::Glyph5x7& glyph = kernel::Font5x7::glyph_for(' ');

    for (uint8_t row : glyph.rows) {
        EXPECT_EQ(row, 0u);
    }
}

TEST(Font5x7Test, UsesFallbackForUnsupportedCharacters) {
    EXPECT_FALSE(kernel::Font5x7::has_glyph('\n'));
    EXPECT_TRUE(glyph_equals(kernel::Font5x7::glyph_for('\n'), kernel::Font5x7::fallback_glyph()));
}

TEST(MemoryMapViewTest, ClassifiesRegionKinds) {
    EXPECT_TRUE(kernel::memory::is_allocatable(kernel::memory::MemoryRegionKind::Usable));
    EXPECT_FALSE(
        kernel::memory::is_allocatable(kernel::memory::MemoryRegionKind::BootloaderReclaimable));
    EXPECT_TRUE(
        kernel::memory::is_reclaimable(kernel::memory::MemoryRegionKind::BootloaderReclaimable));
    EXPECT_TRUE(kernel::memory::is_reclaimable(kernel::memory::MemoryRegionKind::AcpiReclaimable));
    EXPECT_TRUE(kernel::memory::is_reserved(kernel::memory::MemoryRegionKind::Framebuffer));
    EXPECT_TRUE(kernel::memory::is_reserved(kernel::memory::MemoryRegionKind::KernelAndModules));
}

TEST(MemoryMapViewTest, SummarizesMemoryByKind) {
    const kernel::memory::MemoryRegion regions[] = {
        {0x1000, 0x3000, kernel::memory::MemoryRegionKind::Usable},
        {0x4000, 0x1000, kernel::memory::MemoryRegionKind::Reserved},
        {0x5000, 0x2000, kernel::memory::MemoryRegionKind::BootloaderReclaimable},
        {0x7000, 0x4000, kernel::memory::MemoryRegionKind::Framebuffer},
        {0xb000, 0x1000, kernel::memory::MemoryRegionKind::AcpiReclaimable},
    };
    const kernel::memory::MemoryMapStats stats = kernel::memory::MemoryMapView(regions).stats();

    EXPECT_EQ(stats.region_count, 5u);
    EXPECT_EQ(stats.total_bytes, 0xb000u);
    EXPECT_EQ(stats.usable_bytes, 0x3000u);
    EXPECT_EQ(stats.bootloader_reclaimable_bytes, 0x2000u);
    EXPECT_EQ(stats.framebuffer_bytes, 0x4000u);
    EXPECT_EQ(stats.reserved_bytes, 0x2000u);
}

TEST(MemoryMapViewTest, HandlesEmptyMap) {
    const kernel::memory::MemoryMapStats stats = kernel::memory::MemoryMapView().stats();

    EXPECT_EQ(stats.region_count, 0u);
    EXPECT_EQ(stats.total_bytes, 0u);
    EXPECT_EQ(stats.usable_bytes, 0u);
}

TEST(EarlyFrameAllocatorTest, CountsAlignedFrames) {
    const kernel::memory::MemoryRegion aligned{0x1000, 0x3000,
                                               kernel::memory::MemoryRegionKind::Usable};
    const kernel::memory::MemoryRegion unaligned{0x1800, 0x3800,
                                                 kernel::memory::MemoryRegionKind::Usable};
    const kernel::memory::MemoryRegion reserved{0x1000, 0x3000,
                                                kernel::memory::MemoryRegionKind::Reserved};

    EXPECT_EQ(kernel::memory::usable_frame_count(aligned), 3u);
    EXPECT_EQ(kernel::memory::usable_frame_count(unaligned), 3u);
    EXPECT_EQ(kernel::memory::usable_frame_count(reserved), 0u);
}

TEST(EarlyFrameAllocatorTest, AllocatesAcrossUsableRegionsAndSkipsReserved) {
    const kernel::memory::MemoryRegion regions[] = {
        {0x1000, 0x2000, kernel::memory::MemoryRegionKind::Usable},
        {0x3000, 0x4000, kernel::memory::MemoryRegionKind::Reserved},
        {0x7000, 0x1000, kernel::memory::MemoryRegionKind::Framebuffer},
        {0x8000, 0x2000, kernel::memory::MemoryRegionKind::Usable},
    };
    kernel::memory::EarlyFrameAllocator allocator{kernel::memory::MemoryMapView(regions)};
    kernel::memory::PhysicalFrame frame;

    EXPECT_EQ(allocator.stats().total_frames, 4u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address, 0x1000u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address, 0x2000u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address, 0x8000u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address, 0x9000u);
    EXPECT_FALSE(allocator.allocate(frame));
    EXPECT_EQ(allocator.stats().allocated_frames, 4u);
    EXPECT_EQ(allocator.stats().remaining_frames, 0u);
}

TEST(EarlyFrameAllocatorTest, AlignsAllocationsAndSkipsPhysicalZero) {
    const kernel::memory::MemoryRegion regions[] = {
        {0x0000, 0x2000, kernel::memory::MemoryRegionKind::Usable},
        {0x2800, 0x1800, kernel::memory::MemoryRegionKind::Usable},
    };
    kernel::memory::EarlyFrameAllocator allocator{kernel::memory::MemoryMapView(regions)};
    kernel::memory::PhysicalFrame frame;

    EXPECT_EQ(allocator.stats().total_frames, 2u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address, 0x1000u);
    EXPECT_TRUE(allocator.allocate(frame));
    EXPECT_EQ(frame.address, 0x3000u);
    EXPECT_FALSE(allocator.allocate(frame));
}

TEST(EarlyFrameAllocatorTest, ReportsExhaustionOnEmptyMap) {
    kernel::memory::EarlyFrameAllocator allocator;
    kernel::memory::PhysicalFrame frame;

    EXPECT_FALSE(allocator.allocate(frame));
    EXPECT_EQ(allocator.stats().total_frames, 0u);
    EXPECT_EQ(allocator.stats().remaining_frames, 0u);
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

void expect_editor_cell(kernel::EditorViewCell actual, uint64_t column, uint64_t row) {
    EXPECT_EQ(actual.column, column);
    EXPECT_EQ(actual.row, row);
}

TEST(EditorViewLayoutTest, MapsCursorWithinFirstVisualRow) {
    const kernel::EditorViewLayout layout(10, 0, 2);

    expect_editor_cell(layout.position_for(0), 2, 0);
    expect_editor_cell(layout.position_for(7), 9, 0);
    EXPECT_EQ(layout.visual_rows(7), 1u);
}

TEST(EditorViewLayoutTest, WrapsCursorAfterRowEnd) {
    const kernel::EditorViewLayout layout(10, 0, 2);

    expect_editor_cell(layout.position_for(8), 0, 1);
    expect_editor_cell(layout.position_for(18), 0, 2);
    EXPECT_EQ(layout.visual_rows(8), 2u);
    EXPECT_EQ(layout.visual_rows(18), 3u);
}

TEST(EditorViewLayoutTest, AccountsForPromptColumnOffset) {
    const kernel::EditorViewLayout layout(10, 5, 2);

    expect_editor_cell(layout.position_for(0), 7, 0);
    expect_editor_cell(layout.position_for(2), 9, 0);
    expect_editor_cell(layout.position_for(3), 0, 1);
    EXPECT_EQ(layout.visual_rows(3), 2u);
}

TEST(EditorViewLayoutTest, SupportsLongPrompt) {
    const kernel::EditorViewLayout layout(12, 0, 9);

    expect_editor_cell(layout.position_for(0), 9, 0);
    expect_editor_cell(layout.position_for(3), 0, 1);
    expect_editor_cell(layout.position_for(15), 0, 2);
}

TEST(EditorViewLayoutTest, HandlesZeroColumns) {
    const kernel::EditorViewLayout layout(0, 4, 2);

    EXPECT_FALSE(layout.ready());
    expect_editor_cell(layout.position_for(10), 0, 0);
    EXPECT_EQ(layout.visual_rows(10), 1u);
}

void expect_dirty_range(kernel::EditorDirtyRange actual, kernel::EditorDirtyKind kind, size_t start,
                        size_t old_end, size_t new_end) {
    EXPECT_EQ(actual.kind, kind);
    EXPECT_EQ(actual.start, start);
    EXPECT_EQ(actual.old_end, old_end);
    EXPECT_EQ(actual.new_end, new_end);
}

TEST(EditorDirtyRangeTest, AppendAtEndRedrawsOnlyNewCharacter) {
    const kernel::EditorDirtyRange dirty =
        kernel::editor_dirty_range(kernel::EditorEditKind::Insert, {5, 5, 2}, {6, 6, 2});

    expect_dirty_range(dirty, kernel::EditorDirtyKind::Partial, 5, 5, 6);
}

TEST(EditorDirtyRangeTest, InsertInMiddleRedrawsFromInsertionPoint) {
    const kernel::EditorDirtyRange dirty =
        kernel::editor_dirty_range(kernel::EditorEditKind::Insert, {2, 5, 2}, {3, 6, 2});

    expect_dirty_range(dirty, kernel::EditorDirtyKind::Partial, 2, 5, 6);
}

TEST(EditorDirtyRangeTest, BackspaceRedrawsFromRemovedCharacter) {
    const kernel::EditorDirtyRange dirty =
        kernel::editor_dirty_range(kernel::EditorEditKind::Backspace, {3, 5, 2}, {2, 4, 2});

    expect_dirty_range(dirty, kernel::EditorDirtyKind::Partial, 2, 5, 4);
}

TEST(EditorDirtyRangeTest, DeleteRedrawsFromCursor) {
    const kernel::EditorDirtyRange dirty =
        kernel::editor_dirty_range(kernel::EditorEditKind::DeleteForward, {2, 5, 2}, {2, 4, 2});

    expect_dirty_range(dirty, kernel::EditorDirtyKind::Partial, 2, 5, 4);
}

TEST(EditorDirtyRangeTest, CursorMoveDoesNotRedrawText) {
    const kernel::EditorDirtyRange dirty =
        kernel::editor_dirty_range(kernel::EditorEditKind::CursorMove, {4, 5, 2}, {3, 5, 2});

    expect_dirty_range(dirty, kernel::EditorDirtyKind::CursorOnly, 3, 5, 5);
}

TEST(EditorDirtyRangeTest, PromptWidthChangeRequiresFullRedraw) {
    const kernel::EditorDirtyRange dirty =
        kernel::editor_dirty_range(kernel::EditorEditKind::Insert, {5, 5, 2}, {6, 6, 9});

    expect_dirty_range(dirty, kernel::EditorDirtyKind::Full, 0, 0, 0);
}

TEST(EditorDirtyRangeTest, MultilineRangeCanCrossVisualRows) {
    const kernel::EditorViewLayout layout(10, 0, 2);
    const kernel::EditorDirtyRange dirty =
        kernel::editor_dirty_range(kernel::EditorEditKind::Insert, {7, 9, 2}, {8, 10, 2});

    expect_dirty_range(dirty, kernel::EditorDirtyKind::Partial, 7, 9, 10);
    expect_editor_cell(layout.position_for(dirty.start), 9, 0);
    expect_editor_cell(layout.position_for(dirty.new_end), 2, 1);
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
    expect_command(" input ", kernel::ShellCommandKind::Input, "input", "input");
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
    expect_command("input now", kernel::ShellCommandKind::Unknown, "input now", "input");
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

void expect_character_key(kernel::keyboard::KeyboardDecoder& decoder, uint8_t scancode,
                          char expected) {
    const kernel::keyboard::KeyEvent event = expect_key(decoder, scancode);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Character);
    EXPECT_EQ(event.character, expected);
    EXPECT_TRUE(event.pressed);
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

TEST(KeyboardDecoderTest, DecodesTabPressOnly) {
    kernel::keyboard::KeyboardDecoder decoder;

    kernel::keyboard::KeyEvent event = expect_key(decoder, 0x0f);
    EXPECT_EQ(event.key, kernel::keyboard::Key::Tab);
    EXPECT_TRUE(event.pressed);
    EXPECT_EQ(event.character, '\0');

    expect_no_key(decoder, 0x8f);
}

TEST(KeyboardDecoderTest, DecodesPunctuationAndShiftedPunctuation) {
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

TEST(PointerStateTest, InitializesAtCenteredPosition) {
    const kernel::PointerState pointer(100, 80, 10, 16);

    EXPECT_EQ(pointer.x(), 45u);
    EXPECT_EQ(pointer.y(), 32u);
}

TEST(PointerStateTest, AppliesMouseDeltasWithInvertedY) {
    kernel::PointerState pointer(100, 80, 10, 16);

    pointer.move_by(5, 7);

    EXPECT_EQ(pointer.x(), 50u);
    EXPECT_EQ(pointer.y(), 25u);
}

TEST(PointerStateTest, ClampsToBounds) {
    kernel::PointerState pointer(100, 80, 10, 16);

    pointer.move_by(-200, 200);
    EXPECT_EQ(pointer.x(), 0u);
    EXPECT_EQ(pointer.y(), 0u);

    pointer.move_by(300, -300);
    EXPECT_EQ(pointer.x(), 90u);
    EXPECT_EQ(pointer.y(), 64u);
}

TEST(PointerStateTest, HandlesPointerLargerThanBounds) {
    kernel::PointerState pointer(8, 8, 10, 16);

    pointer.move_by(10, -10);

    EXPECT_EQ(pointer.x(), 0u);
    EXPECT_EQ(pointer.y(), 0u);
}

} // namespace
