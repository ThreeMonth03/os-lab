#include <gtest/gtest.h>

#include "kernel/display/window_preview.hpp"

namespace
{

bool contains(kernel::display::Rect rect, uint64_t x, uint64_t y)
{
    return !rect.empty() && x >= rect.x && y >= rect.y && x < rect.x + rect.width &&
           y < rect.y + rect.height;
}

bool contained_by_any(const kernel::display::WindowRepaintRegionList & regions,
                      uint64_t x,
                      uint64_t y)
{
    for (size_t index = 0; index < regions.count(); ++index)
    {
        if (contains(regions.at(index), x, y))
        {
            return true;
        }
    }
    return false;
}

kernel::display::WindowPreviewShape preview()
{
    return {{0, 0, 800, 600}, kernel::display::terminal_window_frame_config(true)};
}

} // namespace

TEST(WindowPreviewTest, DamageMatchesPaintedPixels)
{
    const kernel::display::Rect bounds{10, 20, 320, 200};
    const kernel::display::WindowPreviewShape shape = preview();
    const kernel::display::WindowRepaintRegionList damage = shape.damage_for(bounds);

    for (uint64_t y = bounds.y; y < bounds.y + bounds.height; ++y)
    {
        for (uint64_t x = bounds.x; x < bounds.x + bounds.width; ++x)
        {
            const kernel::display::PixelSample sample = shape.sample(bounds, {0x0066d9ff}, x, y);
            if (sample.opaque())
            {
                EXPECT_TRUE(contained_by_any(damage, x, y))
                    << "opaque preview pixel outside damage at " << x << "," << y;
            }
        }
    }
}

TEST(WindowPreviewTest, ResizeHitSlopOutsideBorderIsTransparent)
{
    const kernel::display::Rect bounds{10, 20, 320, 200};
    const kernel::display::WindowPreviewShape shape = preview();

    const uint64_t inside_right_resize_hit_slop_x = bounds.x + bounds.width - 6;
    const uint64_t client_y = bounds.y + 80;

    EXPECT_FALSE(shape.sample(bounds,
                              {0x0066d9ff},
                              inside_right_resize_hit_slop_x,
                              client_y)
                     .opaque());
}

TEST(WindowPreviewTest, DamageDoesNotFillInterior)
{
    const kernel::display::WindowRepaintRegionList damage =
        preview().damage_for({10, 20, 320, 200});

    EXPECT_FALSE(damage.full_screen_fallback());
    EXPECT_LT(damage.total_area(), 320u * 200u);
    EXPECT_GE(damage.count(), 1u);
}
