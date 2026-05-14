#include <stddef.h>

#include <gtest/gtest.h>

#include "kernel/fixed_vector.hpp"
#include "kernel/history.hpp"
#include "kernel/line_editor.hpp"
#include "kernel/string_view.hpp"

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

} // namespace
