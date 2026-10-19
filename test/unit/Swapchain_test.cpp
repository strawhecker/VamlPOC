// S164 — swapchain format/BPP/present/extent/depth negotiation seams (headless).
//
// Tests S164's PURE decision seams (choose_surface_format / choose_present_mode /
// choose_extent / choose_depth_format / negotiate_image_count /
// format_bytes_per_pixel / depth_format_candidates) with fabricated Vulkan
// values — NO live Vulkan runtime, loader, physical device, surface, or
// swapchain (the S160/S162/S163 precedent: CI proves the decision layer headless;
// the LIVE Swapchain::create() path is manual via the POCs, [both apps, not CI]).

#include <vaml/vulkan/Swapchain.h>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for
// the TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan;

namespace {

VkSurfaceFormatKHR fmt(VkFormat f, VkColorSpaceKHR cs = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
{
    return { f, cs };
}

VkSurfaceCapabilitiesKHR caps(std::uint32_t minCount, std::uint32_t maxCount,
                              std::uint32_t minW, std::uint32_t minH,
                              std::uint32_t maxW, std::uint32_t maxH,
                              std::uint32_t currentW = 0xFFFFFFFF, std::uint32_t currentH = 0xFFFFFFFF)
{
    VkSurfaceCapabilitiesKHR c{};
    c.minImageCount = minCount;
    c.maxImageCount = maxCount;
    c.currentExtent = { currentW, currentH };
    c.minImageExtent = { minW, minH };
    c.maxImageExtent = { maxW, maxH };
    return c;
}

} // namespace

TEST_CASE("S164 format_bytes_per_pixel exhaustive BPP table")
{
    CHECK(format_bytes_per_pixel(VK_FORMAT_B8G8R8A8_SRGB) == 4);
    CHECK(format_bytes_per_pixel(VK_FORMAT_B8G8R8A8_UNORM) == 4);
    CHECK(format_bytes_per_pixel(VK_FORMAT_R8G8B8A8_SRGB) == 4);
    CHECK(format_bytes_per_pixel(VK_FORMAT_R8G8B8A8_UNORM) == 4);
    CHECK(format_bytes_per_pixel(VK_FORMAT_D32_SFLOAT) == 4);
    CHECK(format_bytes_per_pixel(VK_FORMAT_D32_SFLOAT_S8_UINT) == 5);
    CHECK(format_bytes_per_pixel(VK_FORMAT_D24_UNORM_S8_UINT) == 4);
    CHECK(format_bytes_per_pixel(VK_FORMAT_D16_UNORM) == 2);
    // Unknown/unsupported -> 0 ("unknown BPP").
    CHECK(format_bytes_per_pixel(VK_FORMAT_UNDEFINED) == 0);
    CHECK(format_bytes_per_pixel(static_cast<VkFormat>(0xdeadbeef)) == 0);
}

TEST_CASE("S164 choose_surface_format prefers 8-bit BGRA SRGB")
{
    VkSurfaceFormatKHR in[] = {
        fmt(VK_FORMAT_B8G8R8A8_UNORM),
        fmt(VK_FORMAT_B8G8R8A8_SRGB),
        fmt(VK_FORMAT_R8G8B8A8_UNORM),
    };
    auto chosen = choose_surface_format(in);
    CHECK(chosen.format == VK_FORMAT_B8G8R8A8_SRGB);
    CHECK(chosen.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
}

TEST_CASE("S164 choose_surface_format falls back to first concrete when no BGRA SRGB")
{
    VkSurfaceFormatKHR in[] = {
        fmt(VK_FORMAT_R8G8B8A8_UNORM),
        fmt(VK_FORMAT_B8G8R8A8_UNORM),  // BGRA but NOT SRGB
    };
    auto chosen = choose_surface_format(in);
    CHECK(chosen.format == VK_FORMAT_R8G8B8A8_UNORM);  // first available
}

TEST_CASE("S164 choose_surface_format honors UNDEFINED sentinel (skips it)")
{
    VkSurfaceFormatKHR in[] = {
        fmt(VK_FORMAT_UNDEFINED),
        fmt(VK_FORMAT_B8G8R8A8_UNORM),
    };
    auto chosen = choose_surface_format(in);
    // The UNDEFINED "pick anything" entry must NOT be returned; the first
    // concrete format wins.
    CHECK(chosen.format == VK_FORMAT_B8G8R8A8_UNORM);
}

TEST_CASE("S164 choose_surface_format handles empty + all-undefined")
{
    auto empty = choose_surface_format({});
    CHECK(empty.format == VK_FORMAT_UNDEFINED);

    VkSurfaceFormatKHR undefs[] = { fmt(VK_FORMAT_UNDEFINED) };
    auto allUndef = choose_surface_format(undefs);
    CHECK(allUndef.format == VK_FORMAT_UNDEFINED);
}

TEST_CASE("S164 choose_present_mode prefers MAILBOX")
{
    VkPresentModeKHR in[] = { VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR };
    CHECK(choose_present_mode(in) == VK_PRESENT_MODE_MAILBOX_KHR);
}

TEST_CASE("S164 choose_present_mode falls back to FIFO when no mailbox")
{
    VkPresentModeKHR in[] = { VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    CHECK(choose_present_mode(in) == VK_PRESENT_MODE_FIFO_KHR);
}

TEST_CASE("S164 choose_present_mode falls back to first when no FIFO/mailbox")
{
    // No Playback/mailbox and no FIFO in the set -> the first available wins.
    VkPresentModeKHR other[] = { VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR };
    CHECK(choose_present_mode(other) == VK_PRESENT_MODE_FIFO_RELAXED_KHR);
}

TEST_CASE("S164 choose_present_mode empty -> FIFO (guaranteed mode)")
{
    CHECK(choose_present_mode({}) == VK_PRESENT_MODE_FIFO_KHR);
}

TEST_CASE("S164 choose_extent honors a platform-fixed currentExtent")
{
    auto c = caps(2, 4, 1, 1, 800, 600, 640, 480);  // currentExtent NOT sentinel
    auto e = choose_extent(c, 1920, 1080);
    CHECK(e.width == 640);
    CHECK(e.height == 480);
}

TEST_CASE("S164 choose_extent clamps requested size to min..max")
{
    // Sentinel currentExtent => clamp requested to the capabilities range.
    auto c = caps(2, 4, 100, 50, 800, 600);
    auto below = choose_extent(c, 10, 10);   // below min
    CHECK(below.width == 100);
    CHECK(below.height == 50);
    auto above = choose_extent(c, 2000, 3000);  // above max
    CHECK(above.width == 800);
    CHECK(above.height == 600);
}

TEST_CASE("S164 choose_extent passes through in-range + never zero")
{
    auto c = caps(2, 4, 1, 1, 800, 600);
    auto ok = choose_extent(c, 640, 480);  // in-range -> passthrough
    CHECK(ok.width == 640);
    CHECK(ok.height == 480);
    // Zero request clamps to min (or 1 when min is 0) — never zero/negative.
    auto zero = choose_extent(c, 0, 0);
    CHECK(zero.width >= 1);
    CHECK(zero.height >= 1);
}

TEST_CASE("S164 choose_depth_format prefers D32 and handles unsupported")
{
    // Preference order: D32_SFLOAT first.
    VkFormat all[] = {
        VK_FORMAT_D16_UNORM,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT,
    };
    CHECK(choose_depth_format(all) == VK_FORMAT_D32_SFLOAT);

    VkFormat noD32[] = { VK_FORMAT_D16_UNORM, VK_FORMAT_D24_UNORM_S8_UINT };
    CHECK(choose_depth_format(noD32) == VK_FORMAT_D24_UNORM_S8_UINT);
    CHECK(choose_depth_format(depth_format_candidates()) == VK_FORMAT_D32_SFLOAT);
}

TEST_CASE("S164 choose_depth_format empty -> nullopt (skip depth image)")
{
    CHECK(!choose_depth_format({}).has_value());
    // No candidate present in a non-empty span, either.
    VkFormat unrelated[] = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_UNDEFINED };
    CHECK(!choose_depth_format(unrelated).has_value());
}

TEST_CASE("S164 negotiate_image_count clamps to min..max and never zero")
{
    auto c = caps(3, 5, 100, 50, 800, 600);
    CHECK(negotiate_image_count(c, 4) == 4);   // in range
    CHECK(negotiate_image_count(c, 1) == 3);   // below min -> min
    CHECK(negotiate_image_count(c, 9) == 5);   // above max -> max
    CHECK(negotiate_image_count(c, 0) == 3);   // zero -> min

    // maxImageCount == 0 -> driver has no upper bound.
    auto cUnbounded = caps(2, 0, 100, 50, 800, 600);
    CHECK(negotiate_image_count(cUnbounded, 8) == 8);

    // min == 0 + desired 0 -> 1 (never zero).
    auto cZeroMin = caps(0, 4, 0, 0, 800, 600);
    CHECK(negotiate_image_count(cZeroMin, 0) == 1);
}
