#pragma once
// S162 — Vulkan instance (real, permanent), validation layers debug-only.
//
// A real VkInstance RAII owner. Splits the S162 surface into:
//   * PURE DECISION SEAMS (free functions, headless-testable) —
//       required_instance_extensions(bool debug)
//       choose_validation_layers(bool debugEnabled, candidates, available)
//     These are the S162 proof surface proven in ctest WITHOUT a live Vulkan
//     runtime (the standing S160 precedent: CI proves the decision layer
//     headless; the LIVE instance creation is proven manually via the POCs).
//   * VulkanInstance::create(loader, Config) — the real creation path that
//     consumes the seams and constructs the VkInstance (manual/POC-proven).
//
// VALIDATION DEBUG-ONLY: validation layers are enabled ONLY when BOTH
// Config::enableValidation is true AND the build is a debug build (!NDEBUG).
// In release (NDEBUG) the gate is hard-off regardless of Config — the
// validation+xdebugutils surface never ships in release (NO-GAP: S167's
// "no validation errors in debug" consumes this gate).
//
// Layer/extension selection NEVER fails on a missing optional: candidates are
// filtered against what the loader reports as AVAILABLE, and the actually
// enabled set is recorded (enabled_layers()/enabled_extensions()). If a
// requested layer is absent it is dropped, never an error — the robust
// "no validation errors" posture S167 needs.
//
// Lifetimes: the VulkanLoader and the PlatformWindow must OUTLIVE the
// VulkanInstance (destroy order: surface -> instance -> loader). The instance
// must outlive the physical-device/device objects S163+ create from it.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanLoader.h>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The extension names vaml-vulkan's instance requests. In debug this includes
// VK_EXT_DEBUG_UTILS (needed by the S167 debug messenger); in release the list
// is the minimal surface for a swapchain surface (VK_KHR_surface + the
// platform surface extension GLFW selects). Returns std::vector<const char*>.
VAML_VULKAN_EXPORT std::vector<const char*> required_instance_extensions(bool debug);

// Chooses the validation-layer list to enable. Returns the `candidates` that
// are ALSO present in `available`, but ONLY when debugEnabled is true; when
// debugEnabled is false (release/disabled) ALWAYS returns an empty vector —
// the structural "validation is debug-only" gate. `candidates` may be empty.
VAML_VULKAN_EXPORT std::vector<const char*>
choose_validation_layers(bool debugEnabled,
                         std::span<const char* const> candidates,
                         std::span<const char* const> available);

// ---------------------------------------------------------------------------
// Real VkInstance owner (RAII)
// ---------------------------------------------------------------------------

// The S162 instance surface. Constructed only via create(); non-copyable.
class VAML_VULKAN_EXPORT VulkanInstance
{
public:
    struct Config
    {
        const char* appName       = "VAML";
        const char* engineName    = "vaml-vulkan";
        uint32_t    version       = VK_MAKE_API_VERSION(0, 1, 0, 0);
        bool        enableValidation = true; // DEFAULTED from !NDEBUG below
    };

    // Enables validation only when enableValidation && !NDEBUG. Returns
    // nullptr (no throw) if layer loading / vkCreateInstance fails, mirroring
    // PlatformWindow::create — callers never juggle a half-constructed
    // instance. The returned object owns the VkInstance; `loader` must outlive
    // it and all objects derived from it.
    [[nodiscard]] static std::unique_ptr<VulkanInstance>
    create(const VulkanLoader& loader, Config config);

    ~VulkanInstance();
    VulkanInstance(VulkanInstance&&) noexcept;
    VulkanInstance& operator=(VulkanInstance&&) noexcept;
    VulkanInstance(const VulkanInstance&)            = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;

    [[nodiscard]] VkInstance native_handle() const noexcept { return instance_; }
    [[nodiscard]] VkInstance instance()     const noexcept { return instance_; }

    // The layers / extensions ACTUALLY enabled (after the debug gate + the
    // availability filter), in request order. Empty in release.
    [[nodiscard]] const std::vector<const char*>& enabled_layers()     const noexcept { return enabledLayers_; }
    [[nodiscard]] const std::vector<const char*>& enabled_extensions() const noexcept { return enabledExtensions_; }

private:
    VulkanInstance(const VulkanLoader& loader, VkInstance instance,
                   std::vector<const char*> layers,
                   std::vector<const char*> extensions);

    const VulkanLoader* loader_ = nullptr;
    VkInstance instance_        = VK_NULL_HANDLE;
    std::vector<const char*> enabledLayers_;
    std::vector<const char*> enabledExtensions_;
};

} // namespace vaml::vulkan
