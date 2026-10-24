// RESOLVED BY S287 — the S003 MINIMAL REAL tools marker is now the real
// tooling surface (vamlcc + schema/serializer + VBamlReader + CMake
// integration). Definition TU for the vaml-tools version marker; kept + extended
// (the library carries real compiled sources — the embed/TU anchor below is the
// real version identity of the tooling library). The sentinel tag is removed
// (grep-audited — see S287).
#include <vaml/tools/version.h>

// Anchor the translation unit (keeps an empty-TU warning away on all compilers).
namespace vaml::tools {
[[nodiscard]] constexpr std::string_view tools_name_anchor() noexcept
{
    return tools_name();
}
} // namespace vaml::tools
