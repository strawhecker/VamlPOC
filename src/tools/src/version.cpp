// [MINIMAL REAL] tools — RESOLVED BY S287.
// Definition TU for the vaml-tools version marker. Exists so the library has a
// real compiled source (build/link smoke is real, not header-only). Extended, not
// deleted, by S287 when the vamlcc tooling lands.
#include <vaml/tools/version.h>

// Anchor the translation unit (keeps an empty-TU warning away on all compilers).
namespace vaml::tools {
[[nodiscard]] constexpr std::string_view tools_name_anchor() noexcept
{
    return tools_name();
}
} // namespace vaml::tools
