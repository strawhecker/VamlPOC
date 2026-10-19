#pragma once
// S163 — Best-GPU auto-selection (real, permanent; Cat 1 library surface).
//
// The mechanism of choosing a GPU that EVERY future VAML consumer needs
// (headless renderer, app host, S164 device creation) — and therefore a
// genuine library capability (Cat 1), not test-app wiring (Cat 2). The two
// POC apps (VamlPOC.cpp / VamlPOC-linux/main.cpp) both consume these free
// functions and call the SAME select_best_gpu() — "identical code in both
// apps" is satisfied literally because it is one library function, not two
// drift-prone copies.
//
// ALGORITHM (from the S163 roadmap title):
//   1. Score ALL physical devices of the instance (not just the first
//      discrete/integrated): type rank DISCRETE(2) > INTEGRATED(1) > OTHER(0),
//      then device-local memory (descending), then apiVersion (descending) as
//      the tiebreak.
//   2. Filter to present-capable families — a device is present-capable iff
//      some of its queue families supports presentation to the given surface
//      (via vkGetPhysicalDeviceSurfaceSupportKHR). When no surface is
//      supplied (headless/dev), every device is treated as present-capable
//      (an always-present predicate) — documented, not silent.
//   3. VAML_PREFERRED_GPU override — when an environment variable of that
//      name is set and matches a device NAME, that device is FORCED to rank
//      first (explicit user force over the auto-score).
//   4. Log the full ranked list at init (rank / name / type / device-local
//      GiB / apiVersion).
//   5. Return the top present-capable device; if none is present-capable,
//      robust fallback to the top of the full rank (documented).
//
// Split (S162 precedent):
//   * PURE DECISION SEAMS (headless-testable) — rank_physical_devices(...) +
//     the comparator, proven in ctest WITHOUT a live Vulkan runtime.
//   * Real path — select_best_gpu(...), the live enumeration + scoring +
//     override + logging consumed by both POCs.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/VulkanInstance.h>
#include <vaml/vulkan/VulkanSurface.h>
#include <vaml/vulkan/QueueFamilySelection.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::gpu {

// ---------------------------------------------------------------------------
// PURE DECISION SEAM — type/memory/apiVersion ranking (headless-testable)
// ---------------------------------------------------------------------------

// The scoring data for one device. `selected` devices are ranked; the comparer
// needs only `type`, `deviceLocalBytes`, and `apiVersion`. Everything else is
// carried through for rank_physical_devices/select_best_gpu consumers.
struct ScoredDevice
{
    VkPhysicalDevice          device          = VK_NULL_HANDLE;
    std::string               name;
    VkPhysicalDeviceType      type            = VK_PHYSICAL_DEVICE_TYPE_OTHER;
    VkDeviceSize              deviceLocalBytes = 0;
    std::uint32_t             apiVersion      = 0;  // properties.apiVersion
    bool                      presentCapable  = false;
};

// Integer type-strength used by the comparator: DISCRETE=2, INTEGRATED=1,
// OTHER=0. Stable and well-defined.
VAML_VULKAN_EXPORT int device_type_rank(VkPhysicalDeviceType type) noexcept;

// True iff `a` should rank ABOVE `b` (strict weak ordering). Comparator =
//   type rank desc, then deviceLocalBytes desc, then apiVersion desc.
// Deterministic; equal-tie inputs compare less-than in both directions
// (stable — the caller picks the top of a stable-rank).
VAML_VULKAN_EXPORT bool better_device(const ScoredDevice& a, const ScoredDevice& b) noexcept;

// Returns a stable ranking of `devices` (best-first) using better_device.
// Does NOT reorder the callers' span — returns a new vector of the same
// ScoredDevice values ordered best-first. Pure: no logging, no Vulkan calls.
VAML_VULKAN_EXPORT std::vector<ScoredDevice>
rank_physical_devices(std::span<const ScoredDevice> devices);

// ---------------------------------------------------------------------------
// Real path — live selection (consumed by both POCs)
// ---------------------------------------------------------------------------

// The result of a best-GPU selection: the chosen device + the FULL ranked
// list logged at init (rank / name / type / device-local GiB / apiVersion).
struct BestGpuResult
{
    PhysicalDeviceInfo best;       // the chosen device (present-capable, else fallback)
    std::vector<ScoredDevice> ranked;  // every scored device, best-first
};

// Selects the best GPU among ALL PhysicalDeviceInfo of `instance`. See the
// header's ALGORITHM. `surfaceOpt` may be null (headless — all devices are
// present-capable). `preferredGpuName` is the VAML_PREFERRED_GPU value (or
// empty for no override). Logs the full ranked list to stdout. Returns a
// BestGpuResult, or std::nullopt when there are no physical devices.
VAML_VULKAN_EXPORT std::optional<BestGpuResult>
select_best_gpu(const VulkanLoader& loader,
                const VulkanInstance& instance,
                const VulkanSurface* surfaceOpt,
                std::string_view preferredGpuName);

// Reads the VAML_PREFERRED_GPU environment variable (never throws). Empty when
// unset. Separated so the override name is testable/documented independently.
VAML_VULKAN_EXPORT std::string preferred_gpu_env_name();

} // namespace vaml::vulkan::gpu
