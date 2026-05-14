#pragma once

#include "kernel/fixed_vector.hpp"
#include "kernel/string_view.hpp"

namespace kernel {

class LineEditor {
  public:
    static constexpr size_t capacity = 80;

    [[nodiscard]] bool empty() const { return buffer_.empty(); }
    [[nodiscard]] bool full() const { return buffer_.full(); }
    [[nodiscard]] size_t cursor() const { return cursor_; }
    [[nodiscard]] StringView view() const { return {buffer_.data(), buffer_.size()}; }

    [[nodiscard]] bool insert(char value);
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
