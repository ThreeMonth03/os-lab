#pragma once

#include "kernel/base/fixed_vector.hpp"
#include "kernel/shell/shell_limits.hpp"
#include "kernel/base/string_view.hpp"

namespace kernel {

class LineEditor {
  public:
    static constexpr size_t capacity = kShellLineCapacity;
    static constexpr size_t tab_width = 4;

    [[nodiscard]] bool empty() const { return buffer_.empty(); }
    [[nodiscard]] bool full() const { return buffer_.full(); }
    [[nodiscard]] size_t size() const { return buffer_.size(); }
    [[nodiscard]] size_t cursor() const { return cursor_; }
    [[nodiscard]] StringView view() const { return {buffer_.data(), buffer_.size()}; }

    [[nodiscard]] static size_t spaces_to_next_tab_stop(size_t column);

    [[nodiscard]] bool insert(char value);
    [[nodiscard]] bool insert_spaces(size_t count);
    [[nodiscard]] bool replace(StringView value);
    [[nodiscard]] bool backspace();
    [[nodiscard]] bool delete_forward();
    [[nodiscard]] bool move_left();
    [[nodiscard]] bool move_right();
    [[nodiscard]] bool move_to_start();
    [[nodiscard]] bool move_to_end();
    void clear();

  private:
    FixedVector<char, capacity> buffer_;
    size_t cursor_ = 0;
};

} // namespace kernel
