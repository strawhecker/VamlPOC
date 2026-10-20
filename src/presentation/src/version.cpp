// S173/S175 — vaml-presentation version anchor.
//
// Definition TU for the vaml-presentation version marker so the library has a real
// compiled source (build/link smoke is real, not header-only). The version symbol
// is header-only (see version.h); this TU anchors the library's identity. The real
// compiled symbols of this framework are Presenter (S173) + the composition engine
// (S174) — this is the permanent layer marker, NOT a MINIMAL REAL stub.
#include <vaml/presentation/version.h>

// Anchor the translation unit (keeps an empty-TU warning away on all compilers).
namespace vaml::presentation {
[[nodiscard]] constexpr std::string_view presentation_layer_name_anchor() noexcept
{
    return layer_name();
}
} // namespace vaml::presentation
