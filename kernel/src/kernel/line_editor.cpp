#include "kernel/line_editor.hpp"

namespace kernel {

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
