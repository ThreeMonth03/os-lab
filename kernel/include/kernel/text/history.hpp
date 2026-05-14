#pragma once

#include <stddef.h>

#include "kernel/base/fixed_vector.hpp"
#include "kernel/shell/shell_limits.hpp"
#include "kernel/base/string_view.hpp"

namespace kernel
{

enum class HistoryResult
{
    None,
    Command,
    Blank,
};

class History
{
public:
    static constexpr size_t capacity = kShellHistoryCapacity;
    static constexpr size_t entry_capacity = kShellLineCapacity;

    [[nodiscard]] bool empty() const { return count_ == 0; }
    [[nodiscard]] size_t size() const { return count_; }

    [[nodiscard]] bool push(StringView command);
    [[nodiscard]] HistoryResult previous(StringView & command);
    [[nodiscard]] HistoryResult next(StringView & command);
    void reset_browse();

private:
    struct Entry
    {
        [[nodiscard]] bool assign(StringView value);
        [[nodiscard]] StringView view() const { return {characters.data(), characters.size()}; }

        FixedVector<char, entry_capacity> characters;
    };

    Entry entries_[capacity] = {};
    size_t count_ = 0;
    size_t browse_index_ = 0;
    bool browsing_ = false;
};

} // namespace kernel
