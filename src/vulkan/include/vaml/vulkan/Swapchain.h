#pragma once
// S164 — Vulkan swapchain + image views + depth; format/BPP negotiation.
//
// The S164 title delivered as a Cat 1 (library-surface) RAII owner + a set of
// PURE DECISION SEAMS (headless-testable, the S160/S162/S163 precedent — CI
// proves the decision layer; the LIVE swapchain creation is manual via the
// POCs). Depth is a SINGLE raw-Vulkan-device-memory image (bound loader
// entries), NOT a VMA allocator — VMA allocation is a NO-GAP hand-off to S202
// (image upload) where it is genuinely required.
//
// Split (S162 precedent):
//   * PURE DECISION SEAMS (headless-testable, no runtime required) —
//       choose_surface_format  (prefer 8-bit BGRA SRGB; BPP = the format's BPP)
//       choose_present_mode    (prefer MAILBOX, else FIFO, else first, else FIFO)
//       choose_extent          (currentExtent honored, else clamp to min..max)
//       choose_depth_format    (D32 priority; optional when unsupported)
//       negotiate_image_count  (clamp to minImageCount..maxImageCount)
//       format_bytes_per_pixel (the BPP table — 8-bit BGRA = 4, etc.)
//     These are the S164 proof surface proven in ctest WITHOUT a live Vulkan
//     runtime (the standing precedent). The Live creation path consumes them.
//   * `SwapchainConfig` — the negotiated format/colorSpace/presentMode/extent/
//     imageCount/depthFormat (depth sentinel std::nullopt when unsupported).
//   * `Swapchain::create(...)` — the real creation path that queries the
//     surface capabilities/formats/presentModes, runs the seams, creates the
//     VK_KHR_swapchain swapchain (+ the device extension enabled at device
//     creation — part of S164), fetches the images, creates the image views,
//     and creates the depth image + image view + device memory (manual/POC-
//     proven, [both apps, not CI]).
//
// LIFETIMES: the VulkanLoader, the VulkanInstance, the VulkanSurface, AND the
// VkDevice must OUTLIVE the Swapchain. Destroy order: swapchain's depth ->
// swapchain's image views -> the swapchain; the swapchain must be destroyed
// before the device/surface/instance/loader. This is documented (not hidden) —
// callers own the ordering.
//
// NO-GAP recorded (not silent): vkAcquireNextImageKHR/vkQueuePresentKHR belong
// to the S166 draw loop; S165's render pass/framebuffers consume this
// Swapchain's image views; MSAA depth = S168; VMA allocator construction = S202
// (depth uses raw device memory here). RESIZE: S167 delivered destroy-then-create
// Swapchain::recreate(w,h); S167_1 SUPERSEDES that mechanism with the two-generation
// atomic `SwapchainStateMachine` (see SwapchainState.h) — the destructive recreate()
// is REMOVED from this owner (the state builds Gen N+1 into fresh temporaries and
// commits atomically; S167's history entry stays intact as what first shipped).

#include <vaml/vulkan/Vulkan.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/VulkanSurface.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan {

// ---------------------------------------------------------------------------
// PURE DECISION SEAMS (headless-testable; no Vulkan runtime required)
// ---------------------------------------------------------------------------

// Returns the bytes-per-pixel of `format` for the formats vaml-vulkan renders.
// The BPP table is explicit + exhaustive for the supported surface/depth
// formats (8-bit BGRA = 4, D32 = 4, D24X8 = 4, D16 = 2, etc.); any OTHER format
// returns 0 ("unknown BPP"). `format` is a VkFormat.
VAML_VULKAN_EXPORT std::uint32_t format_bytes_per_pixel(VkFormat format) noexcept;

// Chooses the surface format from the formats a surface reports (the S164
// format negotiation headline). Prefers the 8-bit BGRA SRGB format
// (VK_FORMAT_B8G8R8A8_SRGB + VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) so the
// framebuffer is written in linear and the swapchain sRGB-encodes on present
// (correct gamma). Falls back to the FIRST available format otherwise; a
// VK_FORMAT_UNDEFINED entry (the portable "pick anything" sentinel, per the
// Vulkan spec) is honored as "take the next available". An empty span returns
// VK_FORMAT_UNDEFINED.
VAML_VULKAN_EXPORT VkSurfaceFormatKHR
choose_surface_format(std::span<const VkSurfaceFormatKHR> formats);

// Chooses the present mode a surface reports (the S164 present negotiation).
// Prefers MAILBOX (low-latency triple-buffering without tearing), else FIFO
// (the only mode guaranteed by the spec — a safe, always-valid fallback),
// else the FIRST available mode. An empty span returns VK_PRESENT_MODE_FIFO_KHR
// (the guaranteed-present mode) as the defensive fallback.
VAML_VULKAN_EXPORT VkPresentModeKHR
choose_present_mode(std::span<const VkPresentModeKHR> presentModes);

// Chooses the swapchain extent given a surface's capabilities + the requested
// framebuffer size. Honors `currentExtent` when it is NOT the sentinel
// (VK_SURFACE_CURRENT_EXTENT_WIDTH/HEIGHT == 0xFFFFFFFF, i.e. when the platform
// fixes the size) by returning it verbatim. Otherwise clamps the requested
// extent to minImageExtent..maxImageExtent — never below min, never above max,
// never zero (0xFFFFFFFF-flagged components clamped to the capabilities).
VAML_VULKAN_EXPORT VkExtent2D
choose_extent(const VkSurfaceCapabilitiesKHR& caps,
              std::uint32_t framebufferWidth,
              std::uint32_t framebufferHeight);

// Chooses the depth format from the formats a physical device reports as
// supported (probed via vkGetPhysicalDeviceFormatProperties). Prefers
// VK_FORMAT_D32_SFLOAT (32-bit float depth — the S168 MSAA companion), else
// D32_SFLOAT_S8_UINT, D24_UNORM_S8_UINT, D16_UNORM. Returns std::nullopt when
// NO candidate is present in `candidates` (or the span is empty) — the
// S164-recorded "depth image skipped when unsupported" posture.
VAML_VULKAN_EXPORT std::optional<VkFormat>
choose_depth_format(std::span<const VkFormat> candidates);

// Negotiates the swapchain image count. Clamps `desired` to
// minImageCount..maxImageCount (never below the driver floor, never above the
// driver ceiling, never zero — a 0 desired becomes minImageCount). A
// maxImageCount of 0 means "driver has no upper bound".
VAML_VULKAN_EXPORT std::uint32_t
negotiate_image_count(const VkSurfaceCapabilitiesKHR& caps, std::uint32_t desired);

// The candidate depth formats vaml-vulkan tries, in preference order, for
// choose_depth_format. Exposed so a caller can probe support once.
VAML_VULKAN_EXPORT const std::vector<VkFormat>& depth_format_candidates();

// Returns true precisely when `result` (from vkAcquireNextImageKHR or
// vkQueuePresentKHR) means the swapchain is out-of-date or suboptimal and must
// be recreated — the S167 "resize-safe recreation" trigger. VK_SUCCESS and
// every other result return false. Pure + headless-testable (no runtime).
VAML_VULKAN_EXPORT bool should_recreate_swapchain(VkResult result) noexcept;

// ---------------------------------------------------------------------------
// Real swapchain owner (RAII)
// ---------------------------------------------------------------------------

// The negotiated swapchain parameters. Populated by Swapchain::create() and by
// the pure negotiating helper below; exposed via Swapchain accessors.
struct SwapchainConfig
{
    VkSurfaceFormatKHR surfaceFormat  = { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    VkPresentModeKHR   presentMode    = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D         extent         = { 0, 0 };
    std::uint32_t      imageCount     = 0;
    std::optional<VkFormat> depthFormat = std::nullopt;  // std::nullopt => no depth image (unsupported)

    [[nodiscard]] std::uint32_t surface_bytes_per_pixel() const noexcept
    {
        return format_bytes_per_pixel(surfaceFormat.format);
    }
};

// The real S164 swapchain owner: RAII + move-only. Constructed only via
// create(); destroys the depth image/view + image views + the swapchain in
// reverse order on destruction. See the header comment for LIFETIMES.
class VAML_VULKAN_EXPORT Swapchain
{
public:
    // Creates a swapchain on `device` (the S169 Device owner) for `surface`.
    // Queries the surface capabilities/formats/presentModes via `loader`, runs
    // the S164 decision seams, creates the swapchain (VK_KHR_swapchain — the
    // device extension must already be enabled at device creation, which the
    // S169 Device does by default), fetches the images, creates the image views,
    // and creates the depth image/image-view/device-memory. The physical device
    // is borrowed from `device->physical_device()`.
    // Returns nullptr (no throw) on any failure, mirroring
    // PlatformWindow/VulkanInstance/VulkanSurface::create. `loader`, the
    // instance (through `surface`), the `surface`, and `device` must outlive
    // the returned object.
    //
    // `oldSwapchain` is the CURRENT swapchain when this new swapchain REPLACES
    // an existing live one on the same surface (the S167_1 two-generation
    // model: Gen N+1 is built while Gen N is still presentable). Per the Vulkan
    // spec + real-driver behavior (e.g. NVIDIA), a replacement swapchain MUST
    // hand the still-live handle in `VkSwapchainCreateInfoKHR::oldSwapchain`
    // (it replaces atomically) — otherwise creating it while another swapchain
    // for the surface is alive faults. Pass VK_NULL_HANDLE to create the FIRST
    // swapchain for a surface (the common gen-0 case).
    [[nodiscard]] static std::unique_ptr<Swapchain>
    create(const VulkanLoader& loader,
           const Device& device,
           const VulkanSurface& surface,
           std::uint32_t framebufferWidth,
           std::uint32_t framebufferHeight,
           std::optional<SwapchainConfig> configOpt = std::nullopt,
           VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);

    // Convenience: the negotiated parameters WITHOUT creating a swapchain.
    // Pure-ish — consumes the seams + the live surface queries. Exposed so a
    // caller can inspect/adjust the negotiated config before create().
    [[nodiscard]] static std::optional<SwapchainConfig>
    negotiate(const VulkanLoader& loader,
              VkPhysicalDevice physicalDevice,
              const VulkanSurface& surface,
              std::uint32_t framebufferWidth,
              std::uint32_t framebufferHeight,
              std::uint32_t desiredImageCount = 3);

    ~Swapchain();
    Swapchain(Swapchain&&) noexcept;
    Swapchain& operator=(Swapchain&&) noexcept;
    Swapchain(const Swapchain&)            = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    [[nodiscard]] VkSwapchainKHR native_handle() const noexcept { return swapchain_; }
    [[nodiscard]] VkSwapchainKHR swapchain()     const noexcept { return swapchain_; }

    [[nodiscard]] const std::vector<VkImage>&      images()      const noexcept { return images_; }
    [[nodiscard]] const std::vector<VkImageView>&  image_views() const noexcept { return imageViews_; }
    [[nodiscard]] std::uint32_t                    image_count() const noexcept { return static_cast<std::uint32_t>(images_.size()); }

    [[nodiscard]] VkExtent2D         extent()        const noexcept { return config_.extent; }
    [[nodiscard]] VkFormat           color_format()  const noexcept { return config_.surfaceFormat.format; }
    [[nodiscard]] VkColorSpaceKHR    color_space()   const noexcept { return config_.surfaceFormat.colorSpace; }
    [[nodiscard]] VkPresentModeKHR   present_mode()  const noexcept { return config_.presentMode; }
    [[nodiscard]] std::uint32_t      bytes_per_pixel() const noexcept { return config_.surface_bytes_per_pixel(); }

    // Depth: config_.depthFormat is std::nullopt when depth is unsupported
    // (skipped image) — check depth_image() == VK_NULL_HANDLE as well.
    [[nodiscard]] std::optional<VkFormat> depth_format()   const noexcept { return config_.depthFormat; }
    [[nodiscard]] VkImage                 depth_image()    const noexcept { return depthImage_; }
    [[nodiscard]] VkImageView             depth_image_view() const noexcept { return depthImageView_; }
    [[nodiscard]] VkDeviceMemory          depth_memory()   const noexcept { return depthMemory_; }

    [[nodiscard]] const SwapchainConfig& config() const noexcept { return config_; }

private:
    Swapchain(const VulkanLoader& loader, const Device& device);

    // Internal: (re)builds the swapchain + image views + depth from `cfg` into
    // fresh members, destroying any existing members FIRST (reverse order:
    // depth -> image views -> swapchain). Used by create(). `oldSwapchain`, when
    // non-null, is handed to vkCreateSwapchainKHR so the new swapchain replaces
    // it atomically (the S167_1 two-generation path).
    bool rebuild(const SwapchainConfig& cfg, VkSwapchainKHR oldSwapchain);

    const VulkanLoader*    loader_         = nullptr;
    VkDevice               device_         = VK_NULL_HANDLE;  // BORROWED from the Device
    VkPhysicalDevice       physicalDevice_ = VK_NULL_HANDLE;  // BORROWED (instance-owned)
    const VulkanSurface*   surface_        = nullptr;  // must outlive this swapchain (LIFETIMES)
    VkSwapchainKHR         swapchain_      = VK_NULL_HANDLE;

    std::vector<VkImage>     images_;
    std::vector<VkImageView> imageViews_;

    VkImage        depthImage_      = VK_NULL_HANDLE;
    VkImageView    depthImageView_  = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_     = VK_NULL_HANDLE;

    SwapchainConfig config_{};
};

} // namespace vaml::vulkan
