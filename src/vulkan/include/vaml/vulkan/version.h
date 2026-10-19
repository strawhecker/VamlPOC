#pragma once
// VAML vulkan layer version identity.
// RESOLVED at S161 (Sprint 4): the S003 vulkan MINIMAL REAL marker was replaced
// by the real Vulkan/GLFW surface (PlatformWindow, and from S162 the Vulkan
// instance/device/swapchain). The version symbol below is KEPT and extended —
// it is a real, permanent, linkable public symbol alongside PlatformWindow, not
// a stub. The sentinel tag (`// [MINIMAL REAL] vulkan — RESOLVED BY S161`) was
// removed at S161 per the marker contract.

#include <string_view>

namespace vaml::vulkan {

// Semantic version of the vaml-vulkan library.
struct Version
{
    int major = 0;
    int minor = 1;
    int patch = 0;

    [[nodiscard]] constexpr bool operator==(const Version&) const noexcept = default;
};

// Returns the compile-time version of vaml-vulkan.
[[nodiscard]] constexpr Version version() noexcept
{
    return Version{0, 1, 0};
}

// Human-readable layer name.
[[nodiscard]] inline constexpr std::string_view layer_name() noexcept
{
    return "vaml-vulkan";
}

} // namespace vaml::vulkan
