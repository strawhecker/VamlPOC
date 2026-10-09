// S001: definition location for version()/engine_name().
// (Inline constexpr in the header; this TU exists so the library has at least one
// compiled source and the build/link smoke is real, not header-only.)
#include <vaml/version.h>

// Anchor the translation unit (keeps an empty-TU warning away on all compilers).
namespace vaml {
[[nodiscard]] constexpr std::string_view engine_name_anchor() noexcept
{
    return engine_name();
}
} // namespace vaml
