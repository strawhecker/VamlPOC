// S162 — queue-family selection decision seam (headless-safe).
//
// Tests S162's pure select_queue_family / find_present_family seams against a
// FABRICATED VkQueueFamilyProperties array + a mock present-support lambda —
// NO live Vulkan runtime, physical device, or surface (S160 precedent: CI
// proves the decision layer headless; the live device/surface path is manual
// via the POCs). This TU links vaml-vulkan so it also proves the S162
// queue-selection surface links from an external consumer.
//
// Cases cover the headline contract:
//   * unified graphics+present family preferred;
//   * graphics-only fallback when no present-capable family;
//   * nullopt when no graphics family;
//   * present-only families are skipped for the graphics selection but found
//     by find_present_family;
//   * empty/edge inputs.

#include <vaml/vulkan/QueueFamilySelection.h>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for
// the TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan;

namespace {

// Builds a fabricated family array: family i has queueFlags `flags`.
VkQueueFamilyProperties makeFamily(VkQueueFlags flags)
{
    VkQueueFamilyProperties f{};
    f.queueFlags      = flags;
    f.queueCount      = 1;
    f.timestampValidBits = 0;
    return f;
}

} // namespace

TEST_CASE("S162 unified graphics+present family is selected")
{
    // family 0 = compute only; family 1 = graphics+present; family 2 = present only.
    VkQueueFamilyProperties fams[] = {
        makeFamily(VK_QUEUE_COMPUTE_BIT),
        makeFamily(VK_QUEUE_GRAPHICS_BIT),
        makeFamily(0),
    };
    // present-support lambda: only family 1 presents.
    PresentSupportFn present = [](std::uint32_t i) { return i == 1u; };

    auto sel = select_queue_family(fams, 3, present);

    REQUIRE(sel.has_value());
    CHECK(*sel == 1u);
}

TEST_CASE("S162 graphics-only family is the fallback when no present support")
{
    // Two graphics families, but NO family presents.
    VkQueueFamilyProperties fams[] = {
        makeFamily(VK_QUEUE_GRAPHICS_BIT),
        makeFamily(VK_QUEUE_GRAPHICS_BIT),
    };
    PresentSupportFn present = [](std::uint32_t) { return false; };

    auto sel = select_queue_family(fams, 2, present);

    REQUIRE(sel.has_value());
    CHECK(*sel == 0u);  // first graphics family
}

TEST_CASE("S162 returns nullopt when no graphics family")
{
    VkQueueFamilyProperties fams[] = {
        makeFamily(VK_QUEUE_COMPUTE_BIT),
        makeFamily(0),
    };
    PresentSupportFn present = [](std::uint32_t) { return true; };

    auto sel = select_queue_family(fams, 2, present);

    CHECK(!sel.has_value());
}

TEST_CASE("S162 unified preferred over an earlier graphics-only family")
{
    // family 0 = graphics-only (earlier), family 1 = graphics+present.
    VkQueueFamilyProperties fams[] = {
        makeFamily(VK_QUEUE_GRAPHICS_BIT),
        makeFamily(VK_QUEUE_GRAPHICS_BIT),
    };
    PresentSupportFn present = [](std::uint32_t i) { return i == 1u; };

    auto sel = select_queue_family(fams, 2, present);

    REQUIRE(sel.has_value());
    CHECK(*sel == 1u);  // unified preferred over the earlier graphics-only
}

TEST_CASE("S162 no present-capable family found by find_present_family")
{
    // find_present_family needs only the count + the mock predicate, not the
    // family array itself.
    PresentSupportFn present = [](std::uint32_t) { return false; };

    auto p = find_present_family(2, present);
    CHECK(!p.has_value());
}

TEST_CASE("S162 find_present_family returns first present-capable")
{
    // Only the count + mock predicate matter; the family array is irrelevant
    // to this seam (it only considers present-capability via the lambda).
    PresentSupportFn present = [](std::uint32_t i) { return i == 2u; };

    auto p = find_present_family(3, present);
    REQUIRE(p.has_value());
    CHECK(*p == 2u);
}

TEST_CASE("S162 empty / null family inputs are safe (nullopt)")
{
    auto sel = select_queue_family(nullptr, 0, PresentSupportFn());
    CHECK(!sel.has_value());

    auto p = find_present_family(0, PresentSupportFn());
    CHECK(!p.has_value());
}
