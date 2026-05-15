#include "kernel/text/history.hpp"

namespace kernel::text
{

bool History::Entry::assign(StringView value)
{
    if (value.size() > entry_capacity)
    {
        return false;
    }

    characters.clear();
    for (char character : value)
    {
        if (!characters.push_back(character))
        {
            characters.clear();
            return false;
        }
    }

    return true;
}

bool History::push(StringView command)
{
    if (command.empty() || command.size() > entry_capacity)
    {
        return false;
    }

    if (count_ == capacity)
    {
        for (size_t index = 1; index < count_; ++index)
        {
            entries_[index - 1] = entries_[index];
        }
        --count_;
    }

    if (!entries_[count_].assign(command))
    {
        return false;
    }

    ++count_;
    reset_browse();
    return true;
}

HistoryResult History::previous(StringView & command)
{
    command = {};
    if (count_ == 0)
    {
        return HistoryResult::None;
    }

    if (!browsing_)
    {
        browsing_ = true;
        browse_index_ = count_ - 1;
    }
    else if (browse_index_ > 0)
    {
        --browse_index_;
    }

    command = entries_[browse_index_].view();
    return HistoryResult::Command;
}

HistoryResult History::next(StringView & command)
{
    command = {};
    if (!browsing_)
    {
        return HistoryResult::None;
    }

    if (browse_index_ + 1 < count_)
    {
        ++browse_index_;
        command = entries_[browse_index_].view();
        return HistoryResult::Command;
    }

    reset_browse();
    return HistoryResult::Blank;
}

void History::reset_browse()
{
    browsing_ = false;
    browse_index_ = 0;
}

} // namespace kernel::text
