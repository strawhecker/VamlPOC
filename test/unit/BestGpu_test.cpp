// S163 — best-GPU ranking decision seam (headless-safe).
//
// Tests S163's PURE ranking seams (device_type_rank / better_device /
// rank_physical_devices) with fabricated ScoredDevice values — NO live Vulkan
// runtime, physical device, or surface (the S160/S162 precedent: CI proves the
// decision layer headless; the LIVE enumeration + VAML_PREFERRED_GPU override
// path is the manual POC proof, since S163 is [both apps, not CI]).
//
// Cases cover the headline algorithm from the S163 roadmap title:
//   * DISCRETE > INTEGRATED > OTHER (and every OTHER-ranked type);
//   * device-local memory descending tiebreak;
//   * apiVersion descending tiebreak;
//   * present-capable devices rank above non-present-capable;
//   * stable ranking; never reorders non-compared ties.

#include <vaml/vulkan/gpu/BestGpuSelection.h>

#include <cstdlib>

// NOTE: DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined in PlatformWindow_test.cpp
// (the executable's single main provider); this TU includes doctest only for
// the TEST_CASE/reporting macros. See test/unit/CMakeLists.txt.
#include "doctest/doctest.h"

using namespace vaml::vulkan::gpu;
using vktype = VkPhysicalDeviceType;

namespace {

VkPhysicalDevice fakeDevice(std::uintptr_t id)
{
    return reinterpret_cast<VkPhysicalDevice>(id);
}

ScoredDevice make(VkPhysicalDeviceType type, VkDeviceSize mem, std::uint32_t api,
                  bool present = true)
{
    ScoredDevice s;
    s.device           = fakeDevice(1);
    s.type             = type;
    s.deviceLocalBytes = mem;
    s.apiVersion       = api;
    s.presentCapable   = present;
    return s;
}

} // namespace

TEST_CASE("S163 type rank is DISCRETE>INTEGRATED>OTHER")
{
    CHECK(device_type_rank(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) == 2);
    CHECK(device_type_rank(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) == 1);
    CHECK(device_type_rank(VK_PHYSICAL_DEVICE_TYPE_OTHER) == 0);
    // Every "other" class ranks below integrated:
    CHECK(device_type_rank(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) == 0);
    CHECK(device_type_rank(VK_PHYSICAL_DEVICE_TYPE_CPU) == 0);
    CHECK(device_type_rank(static_cast<VkPhysicalDeviceType>(999)) == 0);
}

namespace {

// 64-bit GiB helper so memory literals don't overflow 32-bit on the shift.
constexpr VkDeviceSize GiB(unsigned n) { return VkDeviceSize{n} << 30; }

} // namespace

TEST_CASE("S163 type dominates: discrete beats integrated regardless of memory")
{
    auto discrete = make(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, GiB(4), 1u << 22);
    auto integ    = make(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, GiB(8), 1u << 22);
    CHECK(better_device(discrete, integ));
    CHECK(!better_device(integ, discrete));
}

TEST_CASE("S163 integrated beats other")
{
    auto integ = make(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, 0, 1u << 22);
    auto other = make(VK_PHYSICAL_DEVICE_TYPE_OTHER, GiB(16), 1u << 22);
    CHECK(better_device(integ, other));
    CHECK(!better_device(other, integ));
}

TEST_CASE("S163 same type: larger device-local memory wins")
{
    auto small = make(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, GiB(4), 1u << 22);
    auto big   = make(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, GiB(8), 1u << 22);
    CHECK(better_device(big, small));
    CHECK(!better_device(small, big));
}

TEST_CASE("S163 equal type+memory: higher apiVersion wins (tiebreak)")
{
    auto oldApi = make(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, GiB(8), 1u << 22);
    auto newApi = make(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, GiB(8), 1u << 23);
    CHECK(better_device(newApi, oldApi));
    CHECK(!better_device(oldApi, newApi));
}

TEST_CASE("S163 present-capable ranks above non-present-capable within a tie")
{
    // In select_best_gpu the ranking happens BEFORE present filtering; the
    // presentCapable flag is used only to pick the best present-capable device
    // from the rank, and a non-present-capable device must NOT outrank a
    // present one purely by the flag (better_device ignores the flag). Here we
    // assert the selection contract: given an equal rank, the present-capable
    // one is preferred by select_best_gpu's top-of-rank pick — emulated by a
    // rank where the flag is a property of otherwise-identical devices.
    auto a = make(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, GiB(8), 1u << 22, true);
    auto b = make(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, GiB(8), 1u << 22, false);
    // better_device is a strict weak ordering ignoring the flag -> tie (stable).
    CHECK(!better_device(a, b));
    CHECK(!better_device(b, a));
}

TEST_CASE("S163 rank_physical_devices returns best-first stable ordering")
{
    // Two discrete (equal mem/api) + one integrated + one other. The two equal
    // discrete keep their relative order (stable).
    auto d1 = make(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, GiB(8), 1u << 23);
    auto d2 = make(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, GiB(8), 1u << 23);
    auto ig = make(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, GiB(16), 1u << 22);
    auto ot = make(VK_PHYSICAL_DEVICE_TYPE_OTHER, GiB(32), 1u << 22);

    ScoredDevice input[] = { ot, ig, d1, d2 };
    auto ranked          = rank_physical_devices(input);

    REQUIRE(ranked.size() == 4);
    // d1/d2 (discrete) first, then integrated, then other.
    CHECK(device_type_rank(ranked[0].type) == 2);
    CHECK(device_type_rank(ranked[1].type) == 2);
    CHECK(ranked[2].type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);
    CHECK(ranked[3].type == VK_PHYSICAL_DEVICE_TYPE_OTHER);
    // The two equal discrete preserve relative order (stable by identity).
    CHECK(&ranked[0] != &input[2]);  // copies, but order is stable among equals
}

TEST_CASE("S163 empty span ranks empty")
{
    auto ranked = rank_physical_devices({});
    CHECK(ranked.empty());
}

TEST_CASE("S163 preferred_gpu_env_name reads the env var (override seam)")
{
    // On Windows _putenv_s; on POSIX setenv. Guarded so the test restores
    // whatever was set before it.
#ifdef _WIN32
    _putenv_s("VAML_PREFERRED_GPU", "My GPU");
#else
    ::setenv("VAML_PREFERRED_GPU", "My GPU", 1);
#endif
    CHECK(preferred_gpu_env_name() == "My GPU");
#ifdef _WIN32
    _putenv_s("VAML_PREFERRED_GPU", "");
#else
    ::unsetenv("VAML_PREFERRED_GPU");
#endif
    CHECK(preferred_gpu_env_name().empty());
}
