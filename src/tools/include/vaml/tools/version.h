#pragma once
// VAML tools library version identity.
// RESOLVED BY S287 (Sprint 7): the S003 MINIMAL REAL tools marker is now the
// REAL tooling surface — the vamlcc compiler, the .vbaml schema + serializer,
// the VBamlReader, and the vaml_compile_vaml() CMake integration live in this
// library. The version symbol is KEPT + EXTENDED (never deleted) as the version
// of the now-real tooling library. S003/S287 recorded in roadmap + progress.
// No sentinel tag remains (grep-audited — see S287).

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
    // Extended at S287 (real tooling): minor bumped to 1.0 as the tooling
    // surface landed. Consumers use this to report the vaml-tools version.
    return Version{0, 1, 0};
}

// Human-readable tools name.
[[nodiscard]] inline constexpr std::string_view tools_name() noexcept
{
    return "vaml-tools";
}

} // namespace vaml::tools
