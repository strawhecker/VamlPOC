#pragma once
// VAML presentation layer version identity.
// Sprint 4B (S173-S175): the OPTIONAL presentation/composition framework ABOVE the
// VAML primitives. This is a real, permanent, linkable public symbol alongside the
// Presenter + CompositionEngine — NOT a stub and NOT MINIMAL REAL. It exists so a
// consumer can feature-detect the optional framework (vaml-presentation) and the
// library targets keep a stable per-layer version marker.

#include <string_view>

namespace vaml::presentation {

// Semantic version of the vaml-presentation library.
struct Version
{
    int major = 0;
    int minor = 1;
    int patch = 0;

    [[nodiscard]] constexpr bool operator==(const Version&) const noexcept = default;
};

// Returns the compile-time version of vaml-presentation.
[[nodiscard]] constexpr Version version() noexcept
{
    return Version{0, 1, 0};
}

// Human-readable layer name.
[[nodiscard]] inline constexpr std::string_view layer_name() noexcept
{
    return "vaml-presentation";
}

} // namespace vaml::presentation
