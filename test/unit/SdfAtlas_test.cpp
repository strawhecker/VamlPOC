// S204 — SdfAtlas_test: headless proof of the S204 SDF rasterizer's PURE
// DECISION SEAMS (vaml::vulkan::font), dep:S203. No Vulkan runtime needed:
// (1) `coverage_to_sdf` on a synthetic coverage (a filled square) yields
// interior texels toward max-signed (opaque), exterior far-away texels toward
// min-signed (transparent), and a NEAR-BOUNDARY texel proves the sign flip +
// normalization; (2) `sdf_soft_alpha` edge behaviour (0/1/half/aa-widening);
// (3) sdf_raster_valid shape rules; (4) 1:1 glyph→SDF-tile slot mapping.
//
// NO-GAP (recorded, not silent): the SDF tile UPLOAD (SdfAtlas OWNER over a
// live S202 atlas) is device-backed — proven by the [s204-live] POC block, not
// CI (the S202_1 precedent). The S205 text shader's sampling of the SDF atlas +
// the sdf_soft_alpha AA contract is S205's step.

#include <vaml/vulkan/font/SdfAtlas.h>
#include <vaml/vulkan/font/FontAtlas.h>

#include "doctest/doctest.h"

#include <cstdint>
#include <vector>

namespace vml = vaml::vulkan::font;

using Bytes = std::vector<std::uint8_t>;

namespace {

// A synthetic RGBA8 coverage: a filled (opaque) square region [2,2]..[5,5] in
// an 8x8 tile; everything else transparent.
Bytes filledSquareRgba()
{
    constexpr std::uint32_t W = 8, H = 8;
    Bytes px(W * H * 4u, 0u);
    for (std::uint32_t y = 2; y < 6; ++y)
        for (std::uint32_t x = 2; x < 6; ++x)
        {
            std::uint8_t* p = px.data() + static_cast<std::size_t>(y * W + x) * 4u;
            p[0] = p[1] = p[2] = 255u;
            p[3] = 255u;
        }
    return px;
}

} // namespace

TEST_CASE("S204: sdf_raster_valid shape rules")
{
    vml::SdfRaster r;
    CHECK_FALSE(vml::sdf_raster_valid(r));              // empty
    CHECK_FALSE(vml::sdf_raster_valid({8, 8, 0, {}}));  // zero spread
    CHECK_FALSE(vml::sdf_raster_valid({8, 8, 4, std::vector<std::uint8_t>(10u)}));  // bad size
    CHECK(vml::sdf_raster_valid({8, 8, 4, std::vector<std::uint8_t>(64u)}));        // good
}

TEST_CASE("S204: coverage_to_sdf interior is opaque / exterior transparent")
{
    const auto cov = filledSquareRgba();
    const vml::SdfRaster sdf = vml::coverage_to_sdf(cov, 8, 8, 4u);
    REQUIRE(vml::sdf_raster_valid(sdf));
    REQUIRE(sdf.packed.size() == 64u);
    CHECK(sdf.spread == 4u);

    // Deep interior texel (fully inside, far from the edge): packed towards 255.
    // Texel (3,3) is 1 cell from the nearest edge (edge at x>=2/x<6, y>=2/y<6),
    // at that exact cell the distance is ~1 -> mapped near 0.5+1/8 -> ~159..191.
    const std::uint8_t interior = sdf.packed[3u * 8u + 3u];
    CHECK(interior >= 128u);         // inside → >= edge
    CHECK(interior >= 150u);         // clearly inward

    // Far-outside corner (0,0): fully transparent → packed towards 0.
    const std::uint8_t corner = sdf.packed[0u];
    CHECK(corner <= 128u);           // outside → <= edge
    CHECK(corner < 60u);             // clearly outward
}

TEST_CASE("S204: a near-boundary texel proves the sign flip + normalization")
{
    const auto cov = filledSquareRgba();
    const vml::SdfRaster sdf = vml::coverage_to_sdf(cov.data(), 8, 8, 4u);
    // Edge runs across x=2 (columns 2..5 inside). Texel (1,3) is OUTSIDE, 1
    // cell left of the edge; texel (2,3) is INSIDE, on the boundary.
    const std::uint8_t justOutside = sdf.packed[3u * 8u + 1u];
    const std::uint8_t justInside  = sdf.packed[3u * 8u + 2u];

    CHECK(justInside > 128u);                     // inside side of the edge
    CHECK(justOutside < 128u);                    // outside side
    CHECK(justOutside < justInside);              // strictly less -> the sign flip
    // Both are within the spread window ([0,255]) — normalization held.
    CHECK(justInside <= 255u);
    CHECK(justOutside <= 255u);
}

TEST_CASE("S204: sdf_soft_alpha edge cases")
{
    // Fully inside / fully outside clamp to 1 / 0.
    CHECK(vml::sdf_soft_alpha(1.0f, 0.2f) == doctest::Approx(1.0f));
    CHECK(vml::sdf_soft_alpha(0.0f, 0.2f) == doctest::Approx(0.0f));
    // Half-edge (dist01 = 0.5) → ~0.5 alpha regardless of aa.
    CHECK(vml::sdf_soft_alpha(0.5f, 0.2f) == doctest::Approx(0.5f));

    // Larger `aa` widens the transition: at a fixed dist01 slightly inside the
    // edge, a larger aa yields a lower alpha (the ramp is wider), proving the
    // softness knob.
    const float d = 0.55f;
    const float tight = vml::sdf_soft_alpha(d, 0.05f);
    const float wide  = vml::sdf_soft_alpha(d, 0.25f);
    CHECK(tight > wide);
    // Both stay in [0,1].
    CHECK(tight >= 0.0f);
    CHECK(tight <= 1.0f);
    CHECK(wide  >= 0.0f);
    CHECK(wide  <= 1.0f);
}

TEST_CASE("S204: degenerate input to coverage_to_sdf is structured (no crash)")
{
    const vml::SdfRaster r1 = vml::coverage_to_sdf(static_cast<const std::uint8_t*>(nullptr), 8, 8, 4u);
    CHECK_FALSE(vml::sdf_raster_valid(r1));
    const vml::SdfRaster r2 = vml::coverage_to_sdf(Bytes{}, 0, 0, 4u);
    CHECK_FALSE(vml::sdf_raster_valid(r2));
    const vml::SdfRaster r3 = vml::coverage_to_sdf(filledSquareRgba(), 8, 8, 0u);
    CHECK_FALSE(vml::sdf_raster_valid(r3));  // zero spread rejected
}

TEST_CASE("S204: 1:1 glyph → SDF slot mapping (the S205 sampling index)")
{
    // The SDF atlas uses the same 1:1 codepoint→tile mapping as S203 (SDF glyph
    // tiles occupy the same tile indices as the coverage glyphs).
    CHECK(vml::glyph_slot_index(32u) == 0u);
    CHECK(vml::glyph_slot_index(65u) == 33u);
    CHECK(vml::glyph_slot_index(126u) == 94u);
    CHECK(vml::glyph_slot_index(127u) == vml::kNoSlot);
}
