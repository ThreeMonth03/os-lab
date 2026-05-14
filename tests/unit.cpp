#include <assert.h>
#include <stdio.h>

#include "kernel/history.hpp"
#include "kernel/line_editor.hpp"
#include "kernel/string_view.hpp"

namespace {

bool equals(kernel::StringView left, kernel::StringView right) { return left == right; }

void expect_text(kernel::StringView actual, kernel::StringView expected) {
    assert(equals(actual, expected));
}

void test_line_editor_insert_cursor_and_delete() {
    kernel::LineEditor line;

    assert(line.empty());
    assert(line.cursor() == 0);
    assert(!line.move_left());

    assert(line.insert('a'));
    assert(line.insert('b'));
    assert(line.insert('c'));
    expect_text(line.view(), "abc");
    assert(line.cursor() == 3);
    assert(!line.move_right());

    assert(line.move_left());
    assert(line.move_left());
    assert(line.cursor() == 1);
    assert(line.insert('X'));
    expect_text(line.view(), "aXbc");
    assert(line.cursor() == 2);

    assert(line.backspace());
    expect_text(line.view(), "abc");
    assert(line.cursor() == 1);

    assert(line.delete_forward());
    expect_text(line.view(), "ac");
    assert(line.cursor() == 1);

    assert(line.move_to_start());
    assert(line.cursor() == 0);
    assert(line.delete_forward());
    expect_text(line.view(), "c");
    assert(line.move_to_end());
    assert(line.cursor() == 1);
    assert(line.insert('!'));
    expect_text(line.view(), "c!");

    line.clear();
    assert(line.empty());
    assert(line.cursor() == 0);
}

void test_line_editor_replace() {
    kernel::LineEditor line;

    assert(line.replace("hello"));
    expect_text(line.view(), "hello");
    assert(line.cursor() == line.size());

    assert(line.move_left());
    assert(line.replace("z"));
    expect_text(line.view(), "z");
    assert(line.cursor() == 1);

    char too_long[kernel::LineEditor::capacity + 1] = {};
    for (size_t index = 0; index < kernel::LineEditor::capacity + 1; ++index) {
        too_long[index] = 'x';
    }

    assert(!line.replace(kernel::StringView(too_long, kernel::LineEditor::capacity + 1)));
    expect_text(line.view(), "z");
}

void test_history_browse() {
    kernel::History history;
    kernel::StringView command;

    assert(history.empty());
    assert(history.previous(command) == kernel::HistoryResult::None);
    assert(history.next(command) == kernel::HistoryResult::None);
    assert(!history.push(""));

    assert(history.push("one"));
    assert(history.push("two"));
    assert(history.size() == 2);

    assert(history.previous(command) == kernel::HistoryResult::Command);
    expect_text(command, "two");
    assert(history.previous(command) == kernel::HistoryResult::Command);
    expect_text(command, "one");
    assert(history.previous(command) == kernel::HistoryResult::Command);
    expect_text(command, "one");

    assert(history.next(command) == kernel::HistoryResult::Command);
    expect_text(command, "two");
    assert(history.next(command) == kernel::HistoryResult::Blank);
    assert(command.empty());
    assert(history.next(command) == kernel::HistoryResult::None);

    assert(history.previous(command) == kernel::HistoryResult::Command);
    expect_text(command, "two");
    history.reset_browse();
    assert(history.next(command) == kernel::HistoryResult::None);
}

void test_history_capacity() {
    kernel::History history;
    kernel::StringView command;

    for (size_t index = 0; index < kernel::History::capacity + 1; ++index) {
        const char value[1] = {static_cast<char>('0' + index)};
        assert(history.push({value, 1}));
    }

    assert(history.size() == kernel::History::capacity);
    assert(history.previous(command) == kernel::HistoryResult::Command);
    expect_text(command, "8");

    for (size_t index = 1; index < kernel::History::capacity; ++index) {
        assert(history.previous(command) == kernel::HistoryResult::Command);
    }
    expect_text(command, "1");
}

} // namespace

int main() {
    test_line_editor_insert_cursor_and_delete();
    test_line_editor_replace();
    test_history_browse();
    test_history_capacity();

    puts("unit tests passed");
    return 0;
}
