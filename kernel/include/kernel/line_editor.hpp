#pragma once

#include "kernel/fixed_vector.hpp"
#include "kernel/string_view.hpp"

namespace kernel {

class LineEditor {
  public:
    static constexpr size_t capacity = 80;

    [[nodiscard]] bool empty() const { return buffer_.empty(); }
    [[nodiscard]] bool full() const { return buffer_.full(); }
    [[nodiscard]] StringView view() const { return {buffer_.data(), buffer_.size()}; }

    [[nodiscard]] bool insert(char value);
    [[nodiscard]] bool backspace();
    void clear();

  private:
    FixedVector<char, capacity> buffer_;
};

} // namespace kernel
