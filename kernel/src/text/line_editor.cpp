#include "kernel/text/line_editor.hpp"

namespace kernel {

size_t LineEditor::spaces_to_next_tab_stop(size_t column) {
    const size_t offset = column % tab_width;
    return offset == 0 ? tab_width : tab_width - offset;
}

bool LineEditor::insert(char value) {
    const size_t old_size = buffer_.size();
    if (!buffer_.push_back(value)) {
        return false;
    }

    for (size_t index = old_size; index > cursor_; --index) {
        buffer_[index] = buffer_[index - 1];
    }

    buffer_[cursor_] = value;
    ++cursor_;
    return true;
}

bool LineEditor::insert_spaces(size_t count) {
    if (count > capacity - buffer_.size()) {
        return false;
    }

    for (size_t index = 0; index < count; ++index) {
        (void)insert(' ');
    }

    return true;
}

bool LineEditor::replace(StringView value) {
    if (value.size() > capacity) {
        return false;
    }

    clear();
    for (char character : value) {
        if (!buffer_.push_back(character)) {
            clear();
            return false;
        }
    }

    cursor_ = buffer_.size();
    return true;
}

bool LineEditor::backspace() {
    if (cursor_ == 0) {
        return false;
    }

    --cursor_;
    for (size_t index = cursor_; index + 1 < buffer_.size(); ++index) {
        buffer_[index] = buffer_[index + 1];
    }

    return buffer_.pop_back();
}

bool LineEditor::delete_forward() {
    if (cursor_ >= buffer_.size()) {
        return false;
    }

    for (size_t index = cursor_; index + 1 < buffer_.size(); ++index) {
        buffer_[index] = buffer_[index + 1];
    }

    return buffer_.pop_back();
}

bool LineEditor::move_left() {
    if (cursor_ == 0) {
        return false;
    }

    --cursor_;
    return true;
}

bool LineEditor::move_right() {
    if (cursor_ >= buffer_.size()) {
        return false;
    }

    ++cursor_;
    return true;
}

bool LineEditor::move_to_start() {
    if (cursor_ == 0) {
        return false;
    }

    cursor_ = 0;
    return true;
}

bool LineEditor::move_to_end() {
    if (cursor_ == buffer_.size()) {
        return false;
    }

    cursor_ = buffer_.size();
    return true;
}

void LineEditor::clear() {
    buffer_.clear();
    cursor_ = 0;
}

} // namespace kernel
