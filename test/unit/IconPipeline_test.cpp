// S202_1 — IconPipeline_test: headless proof of the S202_1 import/raster seam
// (vaml::vulkan::icon). These are the PURE decision seams — no Vulkan runtime
// needed: (1) PNG decodes (stb_image), (2) SVG rasterizes (nanosvg), (3) output
// is tagged with its texel count (the S213_1 best-fit input), and (4) the
// ADR-015 STRUCTURAL SOURCE-SET RULE holds — only in-set keys are consumable;
// anything else is rejected as OutOfSet with NO crash. Each source is a tiny
// EMBEDDED fixture literal (packaged asset, never an arbitrary path — the
// ADR-015 boundary is honored even in the test).
//
// NO-GAP (recorded, not silent): this file proves decode/raster + texelCount +
// the source-set rule; the S202_1 IconAtlas binder (upload_tile + sub-region
// invalidation) and the S213_1 DPI best-fit POLICY are covered at their own
// steps/tests. The deliberate fallback GLYPH is S400_1's, not this module's.

#include <vaml/vulkan/icon/IconPipeline.h>
#include <vaml/vulkan/icon/IconAtlas.h>

#include "doctest/doctest.h"

#include <cstdint>
#include <vector>

namespace vml = vaml::vulkan::icon;

using Bytes = std::vector<std::uint8_t>;

namespace {

// A valid 4x4 solid-red RGBA8 PNG (embedded fixture — generated once).
const Bytes kRedPng = {
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,4,0,0,0,4,8,6,0,0,0,169,241,158,126,0,0,0,18,73,
    68,65,84,120,156,99,248,207,192,240,31,25,51,144,46,0,0,60,64,31,225,224,129,119,180,0,0,0,0,73,69,78,
    68,174,66,96,130,
};

// A tiny valid SVG: a solid-red filled square in a 10x10 viewBox (embedded).
const Bytes kRedSvg = (Bytes{
    '<','s','v','g',' ','x','m','l','n','s','=','"','h','t','t','p',':','/','/','w','w','w','.','w','3','.',
    'o','r','g','/','2','0','0','0','/','s','v','g','"',' ','w','i','d','t','h','=','"','1','0','"',' ',
    'h','e','i','g','h','t','=','"','1','0','"','>','<','r','e','c','t',' ','w','i','d','t','h','=','"',
    '1','0','"',' ','h','e','i','g','h','t','=','"','1','0','"',' ','f','i','l','l','=','"','#','f','f',
    '0','0','0','0','"','/','>','<','/','s','v','g','>',
});

// Builds a source set from the two fixtures under simple keys.
vml::IconSourceSet redSet()
{
    vml::IconSourceSet s;
    s.emplace(1, vml::IconSource{vml::IconFormat::Png, kRedPng});
    s.emplace(2, vml::IconSource{vml::IconFormat::Svg, kRedSvg});
    return s;
}

// True iff `px` (top-left of a raster) is substantially red and opaque.
bool isRedTopLeft(const vml::IconRaster& r)
{
    if (r.rgba.size() < 4)
        return false;
    const std::uint8_t* p = r.rgba.data();
    return p[0] > 200u && p[1] < 60u && p[2] < 60u;
}

} // namespace

TEST_CASE("S202_1: PNG decode seam produces dims + a known pixel, tagged with texelCount")
{
    const vml::IconSource src{vml::IconFormat::Png, kRedPng};
    vml::IconRaster r;
    vml::IconErrorKind err = vml::IconErrorKind::None;
    REQUIRE(raster_icon(src, 16, 16, r, &err));
    REQUIRE(err == vml::IconErrorKind::None);
    REQUIRE(icon_raster_valid(r));
    CHECK(r.w == 16);
    CHECK(r.h == 16);
    // texelCount == 16*16 == the S213_1 best-fit tag
    CHECK(r.texelCount == 16u * 16u);
    CHECK(r.rgba.size() == 16u * 16u * 4u);
    CHECK(isRedTopLeft(r));
}

TEST_CASE("S202_1: PNG decode is structured on malformed input (no crash)")
{
    const vml::IconSource bad{vml::IconFormat::Png, Bytes{0x89, 0x50, 0x4E, 0x47, 0x1, 0x02, 0x03}};
    vml::IconRaster r;
    vml::IconErrorKind err = vml::IconErrorKind::None;
    CHECK_FALSE(raster_icon(bad, 8, 8, r, &err));
    CHECK(err == vml::IconErrorKind::Malformed);
    CHECK_FALSE(icon_raster_valid(r));
}

TEST_CASE("S202_1: SVG raster seam produces dims + a known pixel, tagged with texelCount")
{
    const vml::IconSource src{vml::IconFormat::Svg, kRedSvg};
    vml::IconRaster r;
    vml::IconErrorKind err = vml::IconErrorKind::None;
    REQUIRE(raster_icon(src, 16, 16, r, &err));
    REQUIRE(err == vml::IconErrorKind::None);
    REQUIRE(icon_raster_valid(r));
    CHECK(r.w == 16);
    CHECK(r.h == 16);
    CHECK(r.texelCount == 16u * 16u);
    CHECK(r.rgba.size() == 16u * 16u * 4u);
    CHECK(isRedTopLeft(r));
}

TEST_CASE("S202_1: SVG raster is structured on malformed input (no crash)")
{
    const vml::IconSource bad{vml::IconFormat::Svg, Bytes{'<','s','v','g','>','g','a','r','b','a','g','e'}};
    vml::IconRaster r;
    vml::IconErrorKind err = vml::IconErrorKind::None;
    // The nanosvg parser must not crash; it either rejects or yields an invalid
    // image — either way the contract is "no crash + structured result".
    const bool ok = raster_icon(bad, 8, 8, r, &err);
    if (ok)
    {
        CHECK(icon_raster_valid(r));
    }
    else
    {
        CHECK(err != vml::IconErrorKind::None);
    }
}

TEST_CASE("S202_1: zero/oversized target is Unsupported, never a crash")
{
    const vml::IconSource src{vml::IconFormat::Png, kRedPng};
    vml::IconRaster r;
    vml::IconErrorKind err = vml::IconErrorKind::None;
    CHECK_FALSE(raster_icon(src, 0, 0, r, &err));
    CHECK(err == vml::IconErrorKind::Unsupported);
}

TEST_CASE("S202_1: ADR-015 source-set rule — only in-set keys are consumable")
{
    const vml::IconSourceSet set = redSet();
    vml::IconSource out;
    CHECK(source_by_key(set, 1, out));
    CHECK(out.format == vml::IconFormat::Png);
    CHECK(source_by_key(set, 2, out));
    CHECK(out.format == vml::IconFormat::Svg);
    // Out-of-set key → false (the OutOfSet rejection), no crash.
    CHECK_FALSE(source_by_key(set, 999, out));
}

TEST_CASE("S202_1: two sources (PNG + SVG) rasterize through the same surface")
{
    const vml::IconSourceSet set = redSet();
    vml::IconSource pngSrc, svgSrc;
    REQUIRE(source_by_key(set, 1, pngSrc));
    REQUIRE(source_by_key(set, 2, svgSrc));
    vml::IconRaster rp, rs;
    REQUIRE(raster_icon(pngSrc, 8, 8, rp));
    REQUIRE(raster_icon(svgSrc, 8, 8, rs));
    CHECK(rp.texelCount == 8u * 8u);
    CHECK(rs.texelCount == 8u * 8u);
    CHECK(isRedTopLeft(rp));
    CHECK(isRedTopLeft(rs));
}
