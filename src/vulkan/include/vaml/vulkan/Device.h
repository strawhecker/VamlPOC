#pragma once
// S169 — vaml::vulkan::Device: the logical-device + graphics-queue RAII owner.
//
// S169 closes the LAST raw-ownership gap in the S4 GPU lane. Before S169 the
// VkDevice + VkQueue were constructed and owned RAW in BOTH POCs' createVulkanCtx
// (vkCreateDevice + vkGetDeviceQueue), and every downstream create() took a raw
// VkDevice (and often a raw VkPhysicalDevice) parameter. S169 introduces THIS
// Cat 1 owner, implemented in **device.cpp** — the ONLY translation unit in the
// whole repo that OWNS a raw VkDevice/VkQueue. Every other wrapper holds a
// non-owning `const Device*` and borrows the raw device/queue at the free points.
//
// After S169 neither POC owns a raw Vulkan handle: the app holds only library
// RAII owners (unique_ptr<Device>, unique_ptr<SwapchainStateMachine>, ...).
//
// Split (the S162..S168 pure-seam precedent):
//   * PURE DECISION SEAMS (headless-testable, no runtime required) —
//       describe_device_queue(queueFamily, priority)  (the VkDeviceQueueCreateInfo)
//       device_extensions(useSwapchain)               (exactly {VK_KHR_SWAPCHAIN_EXTENSION_NAME})
//       build_device_create_info(queueInfo, extensions) (wires the ptr arrays)
//     These are the S169 proof surface proven in ctest WITHOUT a live Vulkan
//     runtime (a Device::create needs a real physical device + vkCreateDevice).
//   * `Device::create(...)` — the real creation path: builds the queue create
//     info + extension list, calls vkCreateDevice + vkGetDeviceQueue (manual/POC-
//     proven, [both apps, not CI]; returns nullptr on failure, no throw — the
//     S160/S162/S163 create convention).
//
// LIFETIMES: the VulkanLoader, VulkanInstance, and the VkPhysicalDevice (an
// instance-level, non-owned handle) must OUTLIVE the Device. Every device-bound
// wrapper (Swapchain/RenderPass/Pipeline/Framebuffer/.../SwapchainStateMachine)
// must be destroyed BEFORE the Device (they borrow raw device handles). The
// Device must be destroyed before the loader/instance. Documented (not hidden) —
// callers own the ordering.
//
// NO-GAP recorded (not silent): S169 owns the LOGICAL device + the graphics
// queue + the device-extension list + the queue-family index. The PHYSICAL
// device remains an instance-level (non-owned) handle chosen by S163. S170
// (destruction-order clean) re-verifies Device-led teardown; S5/S210 render-
// thread loop consume the Device/queue views. No MINIMAL REAL introduced here —
// Device is a real, permanent, kept-and-extended surface.

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/QueueFamilySelection.h>

#include <cstdint>
#include <memory>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// The VkDeviceQueueCreateInfo for a single graphics+present queue at
// `queueFamily` with the given priority. Queue count is fixed at 1 (the S4
// single-thread proof creates one graphics queue). Pure constructor — only
// assembles the struct, so it is headless-testable by construction.
// pQueuePriorities points INTO the caller's `queuePriority` storage — the
// parameter is a const-ref (NOT a by-value copy) so that pointer does not dangle;
// the caller must hold the named priority lvalue across vkCreateDevice.
VAML_VULKAN_EXPORT VkDeviceQueueCreateInfo
describe_device_queue(std::uint32_t queueFamily, const float& queuePriority) noexcept;

// The device-extension list Device::create enables. `useSwapchain` true (the
// default, the S164 requirement) -> exactly { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
// false -> an empty vector (a device that will never build a swapchain, e.g. a
// pure-headless compute device). Returns by value (a fresh vector each call).
VAML_VULKAN_EXPORT std::vector<const char*>
device_extensions(bool useSwapchain) noexcept;

// Builds the VkDeviceCreateInfo from a queue-create info + an extension list.
// Wires the pQueueCreateInfos pointer and the ppEnabledExtensionNames array
// (the extension vector's data) into the struct. `queueInfo` must outlive the
// returned struct (the struct references it). Pure constructor — headless-testable.
VAML_VULKAN_EXPORT VkDeviceCreateInfo
build_device_create_info(const VkDeviceQueueCreateInfo& queueInfo,
                         const std::vector<const char*>& extensions) noexcept;

// ---------------------------------------------------------------------------
// Real logical-device owner (RAII) — implemented in device.cpp
// ---------------------------------------------------------------------------

// The S169 logical-device + graphics-queue owner: RAII + move-only, owning the
// VkDevice and its graphics VkQueue. Constructed only via create(); destroys the
// device (vkDestroyDevice) on destruction. It ALSO holds a non-owning copy of the
// chosen PhysicalDeviceInfo so downstream owners (Swapchain, MsaaTarget) can
// borrow the physical device for memory-property probes without a separate raw
// VkPhysicalDevice parameter. See the header comment for LIFETIMES.
class VAML_VULKAN_EXPORT Device
{
public:
    struct Config
    {
        // Constructor (NOT default member initializers — a DMI referenced from the
        // enclosing class's `Config config = Config{}` default argument is an
        // ill-formed form clang rejects; the constructor avoids that entirely).
        constexpr Config() noexcept
            : queuePriority(1.0f), enableSwapchain(true) {}
        constexpr explicit Config(float priority) noexcept
            : queuePriority(priority), enableSwapchain(true) {}

        float queuePriority;
        bool  enableSwapchain;  // false -> no swapchain extension (headless)
    };

    // Creates the logical device on `physicalDevice` (`physical` is the S163
    // chosen device) at `queueFamily`, enabling the S164 swapchain extension by
    // default. Runs the S169 seams (describe_device_queue + device_extensions +
    // build_device_create_info), calls vkCreateDevice + vkGetDeviceQueue.
    // Returns nullptr (no throw) on any failure, mirroring
    // VulkanInstance/Swapchain::create. `loader` and `physical.device` must
    // outlive the returned object.
    [[nodiscard]] static std::unique_ptr<Device>
    create(const VulkanLoader& loader,
           const PhysicalDeviceInfo& physical,
           std::uint32_t queueFamily,
           Config config = Config{});

    ~Device();
    Device(Device&&) noexcept;
    Device& operator=(Device&&) noexcept;
    Device(const Device&)            = delete;
    Device& operator=(const Device&) = delete;

    // Read-only views of the owned handles. These NEVER hand ownership out —
    // they are borrow points for vk* calls that require the raw device/queue.
    [[nodiscard]] VkDevice     device()        const noexcept { return device_; }
    [[nodiscard]] VkDevice     native_handle() const noexcept { return device_; }
    [[nodiscard]] VkQueue      graphics_queue() const noexcept { return graphicsQueue_; }
    [[nodiscard]] std::uint32_t queue_family()  const noexcept { return queueFamily_; }
    // The physical device this logical device was created from (borrowed; owned
    // by the instance, not by this Device).
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept { return physical_.device; }
    [[nodiscard]] const PhysicalDeviceInfo& physical() const noexcept { return physical_; }

private:
    Device(const VulkanLoader& loader, PhysicalDeviceInfo physical,
           std::uint32_t queueFamily);

    const VulkanLoader* loader_ = nullptr;
    PhysicalDeviceInfo  physical_;
    VkDevice            device_        = VK_NULL_HANDLE;
    VkQueue             graphicsQueue_ = VK_NULL_HANDLE;
    std::uint32_t       queueFamily_   = 0;
};

} // namespace vaml::vulkan
