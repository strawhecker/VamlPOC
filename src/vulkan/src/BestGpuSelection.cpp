// S163 — BestGpuSelection implementation.
//
// Implements the S163 ranking seams (pure, headless-testable) + the real
// select_best_gpu() path both POCs consume. See BestGpuSelection.h for the
// full ALGORITHM.

#include <vaml/vulkan/gpu/BestGpuSelection.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace vaml::vulkan::gpu {

int device_type_rank(VkPhysicalDeviceType type) noexcept
{
    switch (type)
    {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 2;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 1;
        default: return 0;  // VIRTUAL_GPU, CPU, OTHER, reserved — all OTHER-ranked
    }
}

bool better_device(const ScoredDevice& a, const ScoredDevice& b) noexcept
{
    const int ar = device_type_rank(a.type);
    const int br = device_type_rank(b.type);
    if (ar != br)
        return ar > br;
    if (a.deviceLocalBytes != b.deviceLocalBytes)
        return a.deviceLocalBytes > b.deviceLocalBytes;
    return a.apiVersion > b.apiVersion;
}

std::vector<ScoredDevice>
rank_physical_devices(std::span<const ScoredDevice> devices)
{
    std::vector<ScoredDevice> ranked(devices.begin(), devices.end());
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const ScoredDevice& a, const ScoredDevice& b) {
                         return better_device(a, b);
                     });
    return ranked;
}

std::string preferred_gpu_env_name()
{
#if defined(_WIN32)
    // MSVC flags std::getenv as unsafe (C4996); use the secure variant.
    char*  buf  = nullptr;
    size_t len  = 0;
    if (_dupenv_s(&buf, &len, "VAML_PREFERRED_GPU") != 0 || !buf)
        return std::string();
    std::string result(buf ? buf : "");
    std::free(buf);
    return result;
#else
    const char* v = std::getenv("VAML_PREFERRED_GPU");
    return v ? std::string(v) : std::string();
#endif
}

namespace {

// Maps a ScoredDevice back to a PhysicalDeviceInfo (the S162 shape) so the
// chosen device can drive S164's device creation.
PhysicalDeviceInfo to_physical_info(const ScoredDevice& s)
{
    PhysicalDeviceInfo info;
    info.device           = s.device;
    info.name             = s.name;
    info.type             = s.type;
    info.deviceLocalBytes = s.deviceLocalBytes;
    // properties.apiVersion reconstructed from the scored copy (kept for the
    // full-properties consumer).
    info.properties.apiVersion = s.apiVersion;
    return info;
}

} // namespace

namespace {

// True iff ANY queue family of `device` presents to `surface` (a real
// vkGetPhysicalDeviceSurfaceSupportKHR probe). When there is no surface
// (headless/dev), every device is treated as present-capable — documented in
// the header.
bool device_present_capable(const VulkanLoader& loader,
                            VkPhysicalDevice device,
                            VkSurfaceKHR surface)
{
    std::uint32_t famCount = 0;
    loader.vkGetPhysicalDeviceQueueFamilyProperties(device, &famCount, nullptr);
    std::vector<VkQueueFamilyProperties> fams(famCount);
    loader.vkGetPhysicalDeviceQueueFamilyProperties(device, &famCount, fams.data());

    // vkGetPhysicalDeviceSurfaceSupportKHR is a device function available via
    // the instance; it is declared on PFN_... need the loader to expose it.
    if (!loader.vkGetPhysicalDeviceSurfaceSupportKHR)
        return true;  // can't probe — assume present-capable (robust default)

    for (std::uint32_t i = 0; i < famCount; ++i)
    {
        VkBool32 supported = VK_FALSE;
        if (loader.vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supported) == VK_SUCCESS &&
            supported == VK_TRUE)
        {
            return true;
        }
    }
    return false;
}

// Renders a device name in the ranked log. VkPhysicalDeviceType names.
const char* type_name(VkPhysicalDeviceType t)
{
    switch (t)
    {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "DISCRETE";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "INTEGRATED";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "VIRTUAL";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
        default: return "OTHER";
    }
}

} // namespace

std::optional<BestGpuResult>
select_best_gpu(const VulkanLoader& loader,
                const VulkanInstance& instance,
                const VulkanSurface* surfaceOpt,
                std::string_view preferredGpuName)
{
    auto infos = enumerate_physical_devices(loader, instance);
    if (infos.empty())
        return std::nullopt;

    const VkSurfaceKHR surface = surfaceOpt ? surfaceOpt->surface() : VK_NULL_HANDLE;

    // --- Score every device: type/memory/apiVersion + present-capable flag ---
    std::vector<ScoredDevice> scored;
    scored.reserve(infos.size());
    for (auto& info : infos)
    {
        ScoredDevice s;
        s.device           = info.device;
        s.name             = info.name;
        s.type             = info.type;
        s.deviceLocalBytes = info.deviceLocalBytes;
        s.apiVersion       = info.properties.apiVersion;
        s.presentCapable   = surfaceOpt ? device_present_capable(loader, info.device, surface)
                                        : true;
        scored.push_back(std::move(s));
    }

    // --- VAML_PREFERRED_GPU override: force a name-match to rank first -------
    if (!preferredGpuName.empty())
    {
        // Find the FIRST name match; stable_move it to the front of `scored`
        // BEFORE ranking so a matching device wins the top rank.
        for (std::size_t i = 0; i < scored.size(); ++i)
        {
            if (scored[i].name == preferredGpuName)
            {
                ScoredDevice match = std::move(scored[i]);
                scored.erase(scored.begin() + static_cast<std::ptrdiff_t>(i));
                scored.insert(scored.begin(), std::move(match));
                break;
            }
        }
    }

    // --- Rank (stable, best-first) ---
    std::vector<ScoredDevice> ranked = rank_physical_devices(scored);

    // --- Pick the best PRESENT-CAPABLE device; fallback to top of rank -------
    PhysicalDeviceInfo best;
    bool foundPresent = false;
    for (auto& r : ranked)
    {
        if (r.presentCapable)
        {
            best = to_physical_info(r);
            foundPresent = true;
            break;
        }
    }
    if (!foundPresent && !ranked.empty())
    {
        best          = to_physical_info(ranked.front());
        foundPresent  = false;  // fallback (documented)
    }

    // --- Log the full ranked list at init ---
    std::printf("[best-gpu] ranked physical devices (%zu):\n", ranked.size());
    for (std::size_t i = 0; i < ranked.size(); ++i)
    {
        const auto& r = ranked[i];
        const double gib = static_cast<double>(r.deviceLocalBytes) / (1024.0 * 1024.0 * 1024.0);
        std::printf("  #%zu  %-16s  %-10s  %6.2f GiB  api %u.%u.%u  %s\n",
                    i + 1,
                    r.name.c_str(),
                    type_name(r.type),
                    gib,
                    (r.apiVersion >> 22) & 0x7f,
                    (r.apiVersion >> 12) & 0x3ff,
                    r.apiVersion & 0xfff,
                    r.presentCapable ? "present" : "no-present");
    }
    std::printf("[best-gpu] selected: %s%s\n",
                best.name.empty() ? "<unnamed>" : best.name.c_str(),
                foundPresent ? "" : "  (fallback — no present-capable device)");

    return BestGpuResult{std::move(best), std::move(ranked)};
}

} // namespace vaml::vulkan::gpu
