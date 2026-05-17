#include <stdint.h>
#include <gtest/gtest.h>
#include "kernel/display/backing_surface.hpp"
#include "kernel/display/display.hpp"
#include "kernel/display/framebuffer_presenter.hpp"
#include "kernel/display/scene_buffer.hpp"

namespace
{

void expect_rect(kernel::display::Rect actual, uint64_t x, uint64_t y, uint64_t width, uint64_t height)
{
    EXPECT_EQ(actual.x, x);
    EXPECT_EQ(actual.y, y);
    EXPECT_EQ(actual.width, width);
    EXPECT_EQ(actual.height, height);
}

kernel::display::PixelSample test_cursor_pixel(uint64_t x, uint64_t y)
{
    if (x == 2 && y == 1)
    {
        return kernel::display::opaque_pixel({99});
    }
    return kernel::display::transparent_pixel();
}

kernel::display::Rect test_cursor_bounds()
{
    return {2, 1, 1, 1};
}

TEST(DisplayTest, ClipsRectToSurfaceBounds)
{
    expect_rect(kernel::display::clip_rect({1, 2, 3, 4}, 10, 10), 1, 2, 3, 4);
    expect_rect(kernel::display::clip_rect({8, 7, 5, 6}, 10, 10), 8, 7, 2, 3);
    expect_rect(kernel::display::clip_rect({10, 0, 1, 1}, 10, 10), 10, 0, 0, 0);
    expect_rect(kernel::display::clip_rect({0, 10, 1, 1}, 10, 10), 0, 10, 0, 0);
}

TEST(DisplayTest, ComputesBoundingRect)
{
    expect_rect(kernel::display::bounding_rect({10, 5, 4, 4}, {12, 8, 8, 2}), 10, 5, 10, 5);
    expect_rect(kernel::display::bounding_rect({}, {1, 2, 3, 4}), 1, 2, 3, 4);
    expect_rect(kernel::display::bounding_rect({5, 6, 7, 8}, {}), 5, 6, 7, 8);
}

TEST(DisplayTest, ComputesIntersectRect)
{
    expect_rect(kernel::display::intersect_rect({10, 5, 4, 4}, {12, 8, 8, 2}), 12, 8, 2, 1);
    expect_rect(kernel::display::intersect_rect({0, 0, 3, 3}, {3, 0, 2, 2}), 0, 0, 0, 0);
    expect_rect(kernel::display::intersect_rect({}, {1, 2, 3, 4}), 0, 0, 0, 0);
    expect_rect(kernel::display::intersect_rect({8, 8, 8, 8}, {0, 0, 10, 10}), 8, 8, 2, 2);
}

TEST(DisplayTest, PutPixelAndFillRectAreClipped)
{
    uint32_t pixels[12] = {};
    kernel::display::Surface surface(pixels, 4, 3, 4 * sizeof(uint32_t));

    surface.put_pixel(1, 1, {3});
    surface.put_pixel(9, 9, {7});
    EXPECT_EQ(pixels[5], 3u);
    EXPECT_EQ(surface.pixel(1, 1).value, 3u);
    EXPECT_EQ(surface.pixel(9, 9).value, 0u);

    surface.fill_rect({2, 1, 9, 9}, {5});
    EXPECT_EQ(pixels[6], 5u);
    EXPECT_EQ(pixels[7], 5u);
    EXPECT_EQ(pixels[10], 5u);
    EXPECT_EQ(pixels[11], 5u);
    EXPECT_EQ(pixels[0], 0u);
}

TEST(DisplayTest, BackingSurfaceUsesAbsoluteBounds)
{
    uint32_t pixels[12] = {};
    kernel::display::BackingSurface surface(pixels, {10, 20, 4, 3}, 4);

    EXPECT_TRUE(surface.ready());
    EXPECT_TRUE(surface.contains(10, 20));
    EXPECT_TRUE(surface.contains(13, 22));
    EXPECT_FALSE(surface.contains(9, 20));
    EXPECT_FALSE(surface.contains(14, 22));

    surface.put_pixel(11, 21, {7});
    EXPECT_EQ(surface.pixel(11, 21).value, 7u);
    EXPECT_EQ(pixels[5], 7u);
    EXPECT_FALSE(surface.sample(9, 20).opaque());
    ASSERT_TRUE(surface.sample(11, 21).opaque());
    EXPECT_EQ(surface.sample(11, 21).color.value, 7u);
}

TEST(DisplayTest, ComputesBackingSurfaceStorageBytes)
{
    size_t bytes = 0;

    ASSERT_TRUE(kernel::display::backing_surface_required_bytes({10, 20, 4, 3}, bytes));
    EXPECT_EQ(bytes, 48u);

    EXPECT_FALSE(kernel::display::backing_surface_required_bytes({}, bytes));
}

TEST(DisplayTest, BackingSurfaceFillRectIsClipped)
{
    uint32_t pixels[12] = {};
    kernel::display::BackingSurface surface(pixels, {10, 20, 4, 3}, 4);

    surface.fill_rect({12, 21, 8, 8}, {5});

    EXPECT_EQ(surface.pixel(12, 21).value, 5u);
    EXPECT_EQ(surface.pixel(13, 21).value, 5u);
    EXPECT_EQ(surface.pixel(12, 22).value, 5u);
    EXPECT_EQ(surface.pixel(13, 22).value, 5u);
    EXPECT_EQ(surface.pixel(11, 21).value, 0u);
}

TEST(DisplayTest, BackingSurfaceCopyRectScrollsPixelsWithinSurface)
{
    uint32_t pixels[12] = {};
    for (uint32_t index = 0; index < 12; ++index)
    {
        pixels[index] = index + 1;
    }
    kernel::display::BackingSurface surface(pixels, {10, 20, 4, 3}, 4);

    const kernel::display::Rect copied = surface.copy_rect({10, 21, 4, 2}, 10, 20);

    expect_rect(copied, 10, 20, 4, 2);
    EXPECT_EQ(surface.pixel(10, 20).value, 5u);
    EXPECT_EQ(surface.pixel(13, 20).value, 8u);
    EXPECT_EQ(surface.pixel(10, 21).value, 9u);
    EXPECT_EQ(surface.pixel(13, 21).value, 12u);
}

TEST(DisplayTest, BackingSurfaceCopyRectHandlesOverlappingCopyDown)
{
    uint32_t pixels[12] = {};
    for (uint32_t index = 0; index < 12; ++index)
    {
        pixels[index] = index + 1;
    }
    kernel::display::BackingSurface surface(pixels, {10, 20, 4, 3}, 4);

    const kernel::display::Rect copied = surface.copy_rect({10, 20, 4, 2}, 10, 21);

    expect_rect(copied, 10, 21, 4, 2);
    EXPECT_EQ(surface.pixel(10, 21).value, 1u);
    EXPECT_EQ(surface.pixel(13, 21).value, 4u);
    EXPECT_EQ(surface.pixel(10, 22).value, 5u);
    EXPECT_EQ(surface.pixel(13, 22).value, 8u);
}

TEST(DisplayTest, BackingSurfaceCopyRectClipsSourceAndDestination)
{
    uint32_t pixels[12] = {};
    for (uint32_t index = 0; index < 12; ++index)
    {
        pixels[index] = index + 1;
    }
    kernel::display::BackingSurface surface(pixels, {10, 20, 4, 3}, 4);

    const kernel::display::Rect copied = surface.copy_rect({9, 20, 4, 3}, 12, 21);

    expect_rect(copied, 13, 21, 1, 2);
    EXPECT_EQ(surface.pixel(13, 21).value, 1u);
    EXPECT_EQ(surface.pixel(13, 22).value, 5u);
    EXPECT_EQ(surface.pixel(12, 21).value, 7u);
}

TEST(DisplayTest, SceneBufferStoresAndCopiesPixels)
{
    uint32_t pixels[12] = {};
    kernel::display::SceneBuffer scene(pixels, {0, 0, 4, 3}, 4);

    scene.fill_rect({0, 0, 4, 3}, {1});
    scene.put_pixel(2, 1, {7});
    const kernel::display::Rect copied = scene.copy_rect({0, 1, 4, 2}, 0, 0);

    expect_rect(copied, 0, 0, 4, 2);
    EXPECT_EQ(scene.pixel(2, 0).value, 7u);
    EXPECT_EQ(scene.sample(2, 0).color.value, 7u);
}

TEST(DisplayTest, PresenterPresentsOnlyRequestedRect)
{
    uint32_t front_pixels[12] = {};
    uint32_t scene_pixels[12] = {};
    for (uint32_t index = 0; index < 12; ++index)
    {
        scene_pixels[index] = index + 1;
    }
    kernel::display::Surface front(front_pixels, 4, 3, 4 * sizeof(uint32_t));
    kernel::display::SceneBuffer scene(scene_pixels, {0, 0, 4, 3}, 4);
    kernel::display::FramebufferPresenter presenter;
    presenter.reset(front, scene);

    ASSERT_TRUE(presenter.present_rect({1, 1, 2, 1}));

    EXPECT_EQ(front.pixel(0, 1).value, 0u);
    EXPECT_EQ(front.pixel(1, 1).value, 6u);
    EXPECT_EQ(front.pixel(2, 1).value, 7u);
    EXPECT_EQ(front.pixel(3, 1).value, 0u);
}

TEST(DisplayTest, PresenterDrawsCursorOverlayWithoutChangingScene)
{
    uint32_t front_pixels[12] = {};
    uint32_t scene_pixels[12] = {};
    for (uint32_t index = 0; index < 12; ++index)
    {
        scene_pixels[index] = index + 1;
    }
    kernel::display::Surface front(front_pixels, 4, 3, 4 * sizeof(uint32_t));
    kernel::display::SceneBuffer scene(scene_pixels, {0, 0, 4, 3}, 4);
    kernel::display::FramebufferPresenter presenter;
    presenter.reset(front, scene);
    presenter.set_cursor_overlay(test_cursor_pixel, test_cursor_bounds);

    ASSERT_TRUE(presenter.present_rect({2, 1, 1, 1}));

    EXPECT_EQ(front.pixel(2, 1).value, 99u);
    EXPECT_EQ(scene.pixel(2, 1).value, 7u);
}

TEST(DisplayTest, PresenterCanCopySceneAndFrontForScroll)
{
    uint32_t front_pixels[12] = {};
    uint32_t scene_pixels[12] = {};
    for (uint32_t index = 0; index < 12; ++index)
    {
        front_pixels[index] = index + 1;
        scene_pixels[index] = index + 1;
    }
    kernel::display::Surface front(front_pixels, 4, 3, 4 * sizeof(uint32_t));
    kernel::display::SceneBuffer scene(scene_pixels, {0, 0, 4, 3}, 4);
    kernel::display::FramebufferPresenter presenter;
    presenter.reset(front, scene);

    ASSERT_TRUE(presenter.copy_scene_rect({0, 1, 4, 2}, 0, 0));

    EXPECT_EQ(scene.pixel(0, 0).value, 5u);
    EXPECT_EQ(scene.pixel(3, 1).value, 12u);
    EXPECT_EQ(front.pixel(0, 0).value, 5u);
    EXPECT_EQ(front.pixel(3, 1).value, 12u);
}

} // namespace
