// S641 — PixelDiff_s641_test.cpp (GIMP-style diff + tolerance band + PNG write).
//
// Sprint 11r (S641): the colored per-pixel diff + auto-locate text tolerance band
// + `full-missing-content` detection (one blank, one present) + the REAL PNG
// write via the vendored stb_image_write seam. Pure + headless (both CI lanes);
// PNG encoding is pure data-munging. The structured scan is authoritative; PNG
// bit-match (S.Deferred-PNG) stays the recorded upgrade path.

#include <vaml/presentation/scan/PixelDiff.h>
#include <vaml/presentation/scan/ScanDeclaration.h>

#include "doctest/doctest.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

using namespace vaml::presentation::scan;
using Buffer = vaml::presentation::Impl::ImageBuffer;   // the S640a CPU surface

namespace {

Buffer make_buffer(std::uint32_t w, std::uint32_t h,
                   std::uint8_t fillR, std::uint8_t fillG, std::uint8_t fillB)
{
    Buffer b;
    b.width = w; b.height = h;
    b.bgra.assign(static_cast<std::size_t>(w) * h * 4u, 0u);
    for (std::size_t i = 0; i < b.bgra.size(); i += 4u)
    {
        b.bgra[i + 0] = fillB;
        b.bgra[i + 1] = fillG;
        b.bgra[i + 2] = fillR;
        b.bgra[i + 3] = 255u;
    }
    return b;
}

} // namespace

TEST_CASE("S641 pixel_diff classifies identical frames as Unchanged")
{
    auto a = make_buffer(8u, 6u, 200u, 100u, 50u);
    auto e = make_buffer(8u, 6u, 200u, 100u, 50u);
    const auto d = pixel_diff(a, e, 0.02);
    CHECK(d.unchanged == 8u * 6u);
    CHECK(d.different == 0u);
    CHECK(d.missing == 0u);
    // retained frames are present for a later quad band
    CHECK(d.actual_bgra.size() == 8u * 6u * 4u);
}

TEST_CASE("S641 pixel_diff flags differing frames as Different (HOT)")
{
    auto a = make_buffer(4u, 4u, 200u, 100u, 50u);
    auto e = make_buffer(4u, 4u, 10u, 10u, 10u);   // large deviation
    const auto d = pixel_diff(a, e, 0.02);
    CHECK(d.different == 16u);
    CHECK(d.unchanged == 0u);
    CHECK(d.missing == 0u);
}

TEST_CASE("S641 pixel_diff detects FULL-MISSING-CONTENT (blank actual, inked expected)")
{
    // actual is BLANK; expected is a solid inked frame -> every pixel is a
    // missing-content red region (the roadmap's "one blank, one present" catch).
    auto a = make_buffer(4u, 4u, 0u, 0u, 0u);
    // make_buffer writes opaque non-black; blank requires alpha 0 OR near-black
    // on transparent. Force blank via alpha 0:
    std::fill(a.bgra.begin(), a.bgra.end(), 0u);   // fully transparent -> blank
    auto e = make_buffer(4u, 4u, 200u, 100u, 50u);
    const auto d = pixel_diff(a, e, 0.02);
    CHECK(d.missing == 16u);   // all 16 pixels are red missing-content
    CHECK(d.unchanged == 0u);
}

TEST_CASE("S641 classify_quad re-evaluates a declared band (blank actual -> missing)")
{
    // Build a small grid where the whole frame is otherwise identical EXCEPT a
    // 2x2 band is blank in actual but inked in expected.
    Buffer a = make_buffer(5u, 5u, 120u, 120u, 120u);
    Buffer e = make_buffer(5u, 5u, 120u, 120u, 120u);
    // blank out a 2x2 region (cols 1..2, rows 1..2) in the ACTUAL frame
    for (std::uint32_t yy = 1; yy < 3; ++yy)
        for (std::uint32_t xx = 1; xx < 3; ++xx)
        {
            std::size_t off = (static_cast<std::size_t>(yy) * 5u + xx) * 4u;
            for (int k = 0; k < 4; ++k) a.bgra[off + k] = 0u;  // transparent -> blank
        }
    auto d = pixel_diff(a, e, 0.02);
    // Whole frame is otherwise identical; the 2x2 blank/inked band is missing-content.
    CHECK(d.missing == 4u);
    CHECK(d.unchanged == 25u - 4u);

    // Now classify the QUAD band over that region again with a tight tolerance:
    // classify_quad re-runs the rule over the retained frames (idempotent, same result).
    classify_quad(d, 1.0f, 1.0f, 2.0f, 2.0f, 0.02);
    CHECK(d.missing == 4u);
    CHECK(d.unchanged == 21u);
}

TEST_CASE("S641 pixel_diff shape mismatch never silently passes (whole Different)")
{
    auto a = make_buffer(4u, 4u, 0u, 0u, 0u);
    auto e = make_buffer(5u, 5u, 0u, 0u, 0u);   // different dims
    const auto d = pixel_diff(a, e, 0.02);
    // 16 pixels (the actual's dims) all classified Different (never a silent pass).
    CHECK(d.different == 16u);
    CHECK(d.unchanged == 0u);
}

TEST_CASE("S641 pixel_diff_encode_png writes real PNG bytes")
{
    auto a = make_buffer(8u, 8u, 10u, 200u, 30u);
    auto e = make_buffer(8u, 8u, 10u, 200u, 30u);
    const auto d = pixel_diff(a, e, 0.02);
    std::vector<std::uint8_t> png;
    CHECK(pixel_diff_encode_png(d, png));
    CHECK_FALSE(png.empty());
    // PNG magic
    REQUIRE(png.size() >= 8u);
    CHECK(png[0] == 0x89u);
    CHECK(png[1] == 'P');
    CHECK(png[2] == 'N');
    CHECK(png[3] == 'G');
}

TEST_CASE("S641 pixel_diff_write_png writes a file (temp) and encode rejects empty")
{
    auto a = make_buffer(4u, 4u, 5u, 5u, 5u);
    auto e = make_buffer(4u, 4u, 5u, 5u, 5u);
    auto d = pixel_diff(a, e, 0.02);
    const std::string path = "s641_diff_tmp.png";
    CHECK(pixel_diff_write_png(d, path));
    // clean up the temp artifact
    std::remove(path.c_str());

    // An empty diff cannot encode (never silent).
    PixelDiff empty;
    std::vector<std::uint8_t> png;
    CHECK_FALSE(pixel_diff_encode_png(empty, png));
}
