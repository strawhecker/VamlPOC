// S592 (dep:S591) — PER-WINDOW CLEAR ALPHA + BACKGROUND COMPOSITION (the PURE
// clear-decision seam). Implements `clear::decide_clear` — the 2-branch WPF
// `HwndTarget.SetBackgroundColor` mirror (see VamlRenderer.h for the full rule).
// Pure function, no Vulkan runtime, no host constant: the clear is DERIVED from
// the resolved Window.Background + AllowsTransparency.

#include <vaml/presentation/VamlRenderer.h>

namespace vaml::presentation::clear {

ClearValue decide_clear(bool allowsTransparency, const vaml::Color* resolvedBackground) noexcept
{
    // 1. A TRANSPARENT window clears to alpha 0 (the compositor shows through) —
    //    WPF forces Colors.Transparent for per-pixel mode regardless of the
    //    authored Window.Background (the S590 risk gate).
    if (allowsTransparency)
    {
        return ClearValue{ 0.0f, 0.0f, 0.0f, 0.0f };
    }

    // 2. An OPAQUE window WITH a resolved Background -> that color, alpha 1.0.
    if (resolvedBackground != nullptr)
    {
        return ClearValue{
            static_cast<float>(resolvedBackground->r) / 255.0f,
            static_cast<float>(resolvedBackground->g) / 255.0f,
            static_cast<float>(resolvedBackground->b) / 255.0f,
            1.0f,   // an opaque window's clear is fully opaque (the authored alpha ignored)
        };
    }

    // 3. An OPAQUE window with NO authored Background -> alpha-0 clear (WPF
    //    paints nothing when Window.Background is its DP-default null; no grey
    //    fabricated — see the header rule's consequence).
    return ClearValue{ 0.0f, 0.0f, 0.0f, 0.0f };
}

} // namespace vaml::presentation::clear
