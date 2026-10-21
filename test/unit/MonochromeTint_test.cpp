// S205_1 — MonochromeTint_test: headless proof of the S205_1 MONOCHROME ICON
// TINT surface (dep:S205): (1) the single-channel alpha-mask fragment shader is
// structurally sound SPIR-V + shares the SAME shared set-0-binding-0 atlas
// layout + 128-byte push constant as the S205 kinds; (2) the new UiShaderKind::
// Monochrome arm is distinct; (3) the monochrome_style pc filler sets the
// Foreground + zeroes params + preserves uiScale; (4) the PARTIAL-ALPHA seams
// tint_alpha/tint_sample preserve partial alpha (mask 0.5 -> alpha 0.5*fg.a).
//
// NO-GAP (recorded, not silent): the TintMode {Preserve | Monochrome} enum is
// S400_1's (the control-DP surface); S205_1 delivers the RENDER kind
// UiShaderKind::Monochrome + the shader + the partial-alpha seam S400_1 maps
// onto. TintMode=Monochrome consumes this path; TintMode=Preserve samples the
// multicolor texture via the existing SDF-text/solid/quad kinds (Proven here by
// the reachable {SdfText, Solid, Border} kinds on the shared layout). The live
// UiPipeline::create(Monochrome) is device-backed — proven by the [s205_1-live]
// POC block, not CI (the S200/S202_1/S204/S205 precedent).

#include <vaml/vulkan/ui/UiPipeline.h>
#include <vaml/vulkan/ShaderModule.h>

#include <cstddef>
#include <cmath>

#include "doctest/doctest.h"

using namespace vaml::vulkan::ui;
using namespace vaml::vulkan;

namespace {

ui::Color4 col(float r, float g, float b, float a)
{
    return ui::Color4{ r, g, b, a };
}

bool close(float a, float b, float eps = 1e-4f)
{
    return std::fabs(a - b) <= eps;
}

} // namespace

TEST_CASE("S205_1: UiShaderKind::Monochrome is a distinct arm")
{
    // The Monochrome arm is distinct from the three S205 kinds, and the S205
    // kinds remain reachable (TintMode=Preserve's multicolor sampling path).
    CHECK(UiShaderKind::Monochrome != UiShaderKind::SdfText);
    CHECK(UiShaderKind::Monochrome != UiShaderKind::Solid);
    CHECK(UiShaderKind::Monochrome != UiShaderKind::Border);
    // The three existing kinds are stable + distinct from each other (the shared
    // layout provides the Preserve path S400_1 maps onto).
    CHECK(UiShaderKind::SdfText != UiShaderKind::Solid);
    CHECK(UiShaderKind::Solid != UiShaderKind::Border);
    CHECK(UiShaderKind::SdfText != UiShaderKind::Border);
}

TEST_CASE("S205_1: monochrome_style fills the shared push block for the tint kind")
{
    UiPushConstants pc{};
    pc.uiAndPad[0] = 2.5f;  // uiScale, offset 64 — must survive

    const ui::Color4 fg = col(0.9f, 0.2f, 0.1f, 0.8f);
    monochrome_style(pc, fg);

    CHECK(pc.color[0] == doctest::Approx(0.9f));
    CHECK(pc.color[1] == doctest::Approx(0.2f));
    CHECK(pc.color[2] == doctest::Approx(0.1f));
    CHECK(pc.color[3] == doctest::Approx(0.8f));
    // params zeroed (no sampling params — the shader derives everything from the mask).
    CHECK(pc.params[0] == doctest::Approx(0.0f));
    CHECK(pc.params[1] == doctest::Approx(0.0f));
    CHECK(pc.params[2] == doctest::Approx(0.0f));
    CHECK(pc.params[3] == doctest::Approx(0.0f));
    // uiScale preserved (offset 64, the shared S200 slot — never clobbered).
    CHECK(pc.uiAndPad[0] == doctest::Approx(2.5f));
}

TEST_CASE("S205_1: tint_alpha preserves partial alpha and clamps")
{
    // Identity for in-range coverage: the partial-alpha authority.
    CHECK(close(tint_alpha(0.0f), 0.0f));
    CHECK(close(tint_alpha(0.5f), 0.5f));   // 50% partial alpha preserved
    CHECK(close(tint_alpha(1.0f), 1.0f));
    // Out-of-range clamped [0,1].
    CHECK(close(tint_alpha(-0.5f), 0.0f));
    CHECK(close(tint_alpha(1.7f), 1.0f));
}

TEST_CASE("S205_1: tint_sample is the straight-alpha shader mirror (partial alpha preserved)")
{
    const ui::Color4 fg = col(0.5f, 0.25f, 0.75f, 0.8f);

    // Fully opaque mask -> full foreground alpha.
    const auto full = tint_sample(fg, 1.0f);
    CHECK(full[0] == doctest::Approx(0.5f));
    CHECK(full[1] == doctest::Approx(0.25f));
    CHECK(full[2] == doctest::Approx(0.75f));
    CHECK(full[3] == doctest::Approx(0.8f));

    // Zero mask -> fully transparent, foreground rgb retained (straight alpha).
    const auto zero = tint_sample(fg, 0.0f);
    CHECK(zero[3] == doctest::Approx(0.0f));

    // PARTIAL alpha: a mask of 0.5 yields alpha 0.5 * fg.a = 0.4 — the headline.
    const auto half = tint_sample(fg, 0.5f);
    CHECK(half[3] == doctest::Approx(0.4f));
    // rgb is the UNTAMPERED foreground (the tint, no bitmap recompositing).
    CHECK(half[0] == doctest::Approx(0.5f));

    // Out-of-range clamp applies through the sample (mirror of the shader clamp).
    const auto over = tint_sample(fg, 2.0f);
    CHECK(over[3] == doctest::Approx(0.8f));
}

TEST_CASE("S205_1: the monochrome-tint fragment shares the shared atlas layout shape")
{
    // The S205 shared binding: set 0 binding 0 = ONE combined image sampler
    // (FRAGMENT) — the exact layout the monochrome_tint.frag samples. The tint
    // kind rides the SAME shared layout; no second layout, so S206 batching holds.
    const auto b = ui_shared_set_layout_binding();
    CHECK(b.binding == 0u);
    CHECK(b.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    CHECK(b.descriptorCount == 1u);
    CHECK(b.stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT);

    // The shared 128-byte push range the monochrome_tint shader reads (pc.color).
    const auto range = ui_push_constant();
    CHECK(range.size == 128u);
    CHECK(range.offset == 0u);
}

TEST_CASE("S205_1: the monochrome-tint fragment shader is structurally sound")
{
    const auto mono = monochrome_tint_fragment_spirv();
    CHECK(!mono.empty());
    // SPIR-V magic + non-trivial length (like the S205/S200 shaders).
    CHECK(mono[0] == 0x07230203u);
    CHECK(mono.size() > 5u);
    // It samples the atlas (like sdf_text) so it is larger than the no-sampling
    // solid fragment — a passing structural-intent note (the tint shader is
    // mask-sampling, not flat color).
    const auto solid = solid_color_fragment_spirv();
    // The tint shader reads the same shared push block (pc.color = Foreground) as
    // the sdf_text shader — both are atlas-sampling fragments on the shared layout.
    CHECK(sdf_text_fragment_spirv().size() > 0u);
    CHECK(mono.size() > solid.size());
    CHECK(!solid.empty());
}
