// [MINIMAL REAL] vulkan — RESOLVED BY S161.
// Definition TU for the vaml-vulkan version marker. Exists so the library has a
// real compiled source (build/link smoke is real, not header-only). Extended, not
// deleted, by S161 when the Vulkan surface lands.
#include <vaml/vulkan/version.h>

// Anchor the translation unit (keeps an empty-TU warning away on all compilers).
namespace vaml::vulkan {
[[nodiscard]] constexpr std::string_view layer_name_anchor() noexcept
{
    return layer_name();
}
} // namespace vaml::vulkan
