#include "kernel/display/present_region_list.hpp"

namespace kernel::display
{

namespace
{

uint64_t rect_area(Rect rect)
{
    return rect.width * rect.height;
}

bool ranges_touch_or_overlap(uint64_t first_origin,
                             uint64_t first_size,
                             uint64_t second_origin,
                             uint64_t second_size)
{
    return first_origin <= second_origin + second_size &&
           second_origin <= first_origin + first_size;
}

bool horizontally_mergeable(Rect lhs, Rect rhs)
{
    return lhs.y == rhs.y && lhs.height == rhs.height &&
           ranges_touch_or_overlap(lhs.x, lhs.width, rhs.x, rhs.width);
}

bool vertically_mergeable(Rect lhs, Rect rhs)
{
    return lhs.x == rhs.x && lhs.width == rhs.width &&
           ranges_touch_or_overlap(lhs.y, lhs.height, rhs.y, rhs.height);
}

bool mergeable(Rect lhs, Rect rhs)
{
    if (lhs.empty() || rhs.empty())
    {
        return false;
    }

    return !intersect_rect(lhs, rhs).empty() || horizontally_mergeable(lhs, rhs) ||
           vertically_mergeable(lhs, rhs);
}

} // namespace

void PresentRegionList::reset(Rect bounds)
{
    bounds_ = bounds;
    clear();
    stats_ = {};
}

void PresentRegionList::clear()
{
    for (Rect & region : regions_)
    {
        region = {};
    }
    count_ = 0;
    full_screen_fallback_ = false;
}

PresentRegionAppendResult PresentRegionList::append(Rect rect)
{
    rect = intersect_rect(bounds_, rect);
    if (rect.empty())
    {
        return PresentRegionAppendResult::Ignored;
    }

    ++stats_.append_count;
    stats_.presented_pixels += rect_area(rect);
    const uint64_t area = rect_area(rect);
    if (area > stats_.largest_region_area)
    {
        stats_.largest_region_area = area;
    }

    if (full_screen_fallback_)
    {
        return PresentRegionAppendResult::FullscreenFallback;
    }

    for (size_t index = 0; index < count_; ++index)
    {
        if (!mergeable(regions_[index], rect))
        {
            continue;
        }

        regions_[index] = bounding_rect(regions_[index], rect);
        while (merge_region_at(index))
        {
        }
        ++stats_.merge_count;
        return PresentRegionAppendResult::Merged;
    }

    if (count_ >= kMaxPresentRegions)
    {
        return fallback_to_fullscreen();
    }

    regions_[count_++] = rect;
    return PresentRegionAppendResult::Queued;
}

Rect PresentRegionList::at(size_t index) const
{
    return index < count_ ? regions_[index] : Rect{};
}

PresentRegionAppendResult PresentRegionList::fallback_to_fullscreen()
{
    clear();
    full_screen_fallback_ = true;
    if (!bounds_.empty())
    {
        regions_[0] = bounds_;
        count_ = 1;
    }
    ++stats_.fallback_count;
    return PresentRegionAppendResult::FullscreenFallback;
}

bool PresentRegionList::merge_region_at(size_t index)
{
    if (index >= count_)
    {
        return false;
    }

    for (size_t other = 0; other < count_; ++other)
    {
        if (other == index || !mergeable(regions_[index], regions_[other]))
        {
            continue;
        }

        regions_[index] = bounding_rect(regions_[index], regions_[other]);
        regions_[other] = regions_[count_ - 1];
        regions_[count_ - 1] = {};
        --count_;
        ++stats_.merge_count;
        return true;
    }
    return false;
}

} // namespace kernel::display
