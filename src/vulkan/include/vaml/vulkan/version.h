#pragma once
// VAML vulkan layer version identity.
// [MINIMAL REAL] vulkan — RESOLVED BY S161 (Sprint 4: real Vulkan surface).
// This is a strategic seed (NOT lazy LLM): it gives vaml-vulkan a real, linkable,
// permanent public symbol so the artifact model (static+shared, export header,
// ADR-011 layer) is observable NOW, and is extended — not deleted — when the real
// Vulkan implementation lands at S161. Do NOT extend silently; do NOT backfill.

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
