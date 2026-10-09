#pragma once
// VAML tools library version identity.
// [MINIMAL REAL] tools — RESOLVED BY S287 (Sprint 7: vamlcc compiler tooling).
// A strategic seed (NOT lazy LLM): gives vaml-tools a real, linkable, permanent
// public symbol so the artifact model is observable NOW, extended — not deleted —
// when the real tooling lands at S287. Do NOT extend silently; do NOT backfill.

#include <string_view>

namespace vaml::tools {

// Semantic version of the vaml-tools library.
struct Version
{
    int major = 0;
    int minor = 1;
    int patch = 0;

    [[nodiscard]] constexpr bool operator==(const Version&) const noexcept = default;
};

// Returns the compile-time version of vaml-tools.
[[nodiscard]] constexpr Version version() noexcept
{
    return Version{0, 1, 0};
}

// Human-readable tools name.
[[nodiscard]] inline constexpr std::string_view tools_name() noexcept
{
    return "vaml-tools";
}

} // namespace vaml::tools
