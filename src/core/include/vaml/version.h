#pragma once
// VAML core version + engine identity.
// S001: real, usable version API (not a stub) so the library baseline compiles.

#include <string_view>

namespace vaml {

// Semantic version of the vaml-core library.
struct Version
{
    int major = 0;
    int minor = 1;
    int patch = 0;

    [[nodiscard]] constexpr bool operator==(const Version&) const noexcept = default;
};

// Returns the compile-time version of vaml-core.
[[nodiscard]] constexpr Version version() noexcept
{
    return Version{0, 1, 0};
}

// Human-readable engine name.
[[nodiscard]] inline constexpr std::string_view engine_name() noexcept
{
    return "vaml-core";
}

} // namespace vaml
