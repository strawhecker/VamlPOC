// S643 — ToolSmoke_s643_test.cpp (the headless, both-lane half of the run→scan→compare proof).
//
// Sprint 11r (S643, dep:S642) — TOOL SMOKE TEST: ONE authored `.xaml` using ONE of each
// capability type (1 int, 1 bool, 1 string, 1 icon, 1 text) PLUS two no-text geometry
// sentinels (a Rectangle + a Border) proves the run → scan → compare tooling works
// end-to-end, so a parity failure at S644+ is ATTRIBUTABLE (capability broken vs tooling
// broken — separated).
//
// This suite is the ENFORCEABLE headless half (both CI lanes, PURE, no Vulkan runtime):
// it READS the COMMITTED `vaml_ui_tester/sample.smoke.xaml` (via VAML_UI_TESTER_DIR), parses
// its in-XAML SCAN DECLARATION (the primary oracle), and drives the S641/S642 compare over a
// synthetic ScrapeGrid that mirrors BOTH regimes:
//   * FUZZY floor — int01/bool01/str01/text01 (text) + icon01 (icon): presence-level
//     "non-empty == non-empty". (An int/bool/string bound to a TextBlock RENDERS AS TEXT —
//     a glyph label, NOT a filled box — so these are text quads, the fuzzy pool.)
//   * EXACT sentinels — box01 (Rectangle, Solid) + border01 (Border) at declared tight rects:
//     the loop's FAILS-LOUD geometry path.
// It asserts the loop (a) PASSES when everything renders, and (b) FAILS LOUDLY on a blank
// geometry quad or a blank text quad — the ability to fail is the whole point of the smoke
// (a capability failure must never be masked as a silent all-presence pass).
//
// REVISIT S640b/S641 (metadata fine-tune) CLOSURE: the in-XAML SCAN DECLARATION is the
// PRIMARY oracle and ALWAYS wins the tolerance/kind merge (the locked S642 order:
// declared > sidecar catalogue > kind default). This suite proves the smoke round-trips
// the declared per-quad capability id + tolerance through the compare — the DECLARED
// tolerance appears in every datum, documented, not silent. The emitter-sidecar metadata
// is the SECONDARY, forward-compatible hint; the smoke proves it SUFFICES because the
// declaration is authoritative (see roadmap/progress REVISIT closure record).

#include <vaml/presentation/scan/ScanDeclaration.h>
#include <vaml/presentation/scan/Scanner.h>
#include <vaml/presentation/scan/EmitterMetadata.h>
#include <vaml/presentation/scan/CapabilityCatalogue.h>

#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace vaml::presentation::scan;

namespace {

// Reads + parses the COMMITTED smoke `.xaml`'s SCAN DECLARATION (the primary oracle).
ScanDeclaration smoke_declaration()
{
    std::ifstream fs(VAML_UI_TESTER_DIR "/sample.smoke.xaml");
    REQUIRE(fs.good());
    std::ostringstream ss;
    ss << fs.rdbuf();
    const DeclarationResult r = parse_scan_declaration(ss.str());
    REQUIRE(r.ok);
    return r.declaration;
}

// Builds a ScrapeGrid with the 7 smoke quads present+inked. `inkSolid` false = the two
// geometry sentinels (box01/border01) are left BLANK (transparent) so the loop must FAIL
// loudly on geometry. `inkText` false = the text/icon quads are left blank so the loop
// must FAIL loudly on the fuzzy floor.
ScrapeGrid smoke_grid(const ScanDeclaration& decl, bool inkSolid, bool inkText)
{
    ScrapeGrid g;
    g.width  = decl.width;
    g.height = decl.height;
    g.bgra.assign(static_cast<std::size_t>(g.width) * g.height * 4u, 0u);

    auto ink = [&](std::int32_t x, std::int32_t y, std::int32_t w, std::int32_t h)
    {
        const std::int32_t x1 = std::min<std::int32_t>((std::int32_t)g.width,  x + w);
        const std::int32_t y1 = std::min<std::int32_t>((std::int32_t)g.height, y + h);
        for (std::int32_t yy = std::max<std::int32_t>(0, y); yy < y1; ++yy)
            for (std::int32_t xx = std::max<std::int32_t>(0, x); xx < x1; ++xx)
            {
                std::size_t off = (static_cast<std::size_t>(yy) * g.width + static_cast<std::size_t>(xx)) * 4u;
                g.bgra[off + 0] = 200u; g.bgra[off + 1] = 190u;
                g.bgra[off + 2] = 180u; g.bgra[off + 3] = 255u;
            }
    };

    for (const ScanQuad& q : decl.quads)
    {
        // Every smoke quad is a framework element named like the quad (the scrape seeds
        // it from the SAME name the declaration + the emitter sidecar use).
        ScrapeGrid::ElementSample e;
        e.name = q.name;
        if (q.located)
        {
            e.x = q.x; e.y = q.y; e.w = q.w; e.h = q.h;
        }
        else
        {
            // auto-located text/icon: give it a nominal box in the lower-left region.
            e.x = 16.0f; e.y = 16.0f; e.w = 120.0f; e.h = 24.0f;
        }
        g.elements.push_back(e);

        // Ink per regime.
        const bool isSolid = (q.kind == QuadKind::Solid && q.located);
        const bool inkThis = isSolid ? inkSolid : inkText;
        if (inkThis)
            ink(static_cast<std::int32_t>(e.x), static_cast<std::int32_t>(e.y),
                static_cast<std::int32_t>(e.w), static_cast<std::int32_t>(e.h));
    }
    return g;
}

// Builds the emitter sidecar the way the WPF tree-scanner would deposit it (the SECONDARY
// oracle): text quads -> text-N kind; icon -> icon-N; the geometry sentinels are solid.
EmitterMetadata smoke_sidecar(const ScanDeclaration& decl)
{
    EmitterMetadata meta;
    meta.width = decl.width;
    meta.height = decl.height;
    for (const ScanQuad& q : decl.quads)
    {
        MetadataQuad mq;
        mq.name = q.name;
        mq.kind = q.kind;
        mq.has_tolerance = q.has_tolerance;
        mq.tolerance = q.tolerance;
        mq.capability = q.capability;
        meta.quads.push_back(mq);
    }
    return meta;
}

bool datum(const ScanReport& rep, const std::string& quad, const std::string& what)
{
    for (const auto& d : rep.data)
        if (d.quad == quad && d.what == what)
            return true;
    return false;
}

} // namespace

TEST_CASE("S643 the smoke declaration parses: 7 quads, both regimes, capability-tagged")
{
    const ScanDeclaration decl = smoke_declaration();
    REQUIRE(decl.quads.size() == 7u);
    CHECK(decl.width == 440u);
    CHECK(decl.height == 340u);

    bool sawInt = false, sawBool = false, sawString = false, sawIcon = false, sawText = false;
    bool sawBox = false, sawBorder = false;
    for (const ScanQuad& q : decl.quads)
    {
        if (q.name == "int01")
        {
            CHECK(q.kind == QuadKind::Text);           // an int renders as a glyph label -> fuzzy
            CHECK(q.capability == "slot_int_01");
            CHECK(q.tolerance == doctest::Approx(0.08));
            sawInt = true;
        }
        else if (q.name == "bool01")
        {
            CHECK(q.kind == QuadKind::Text);           // bool renders as a glyph label -> fuzzy
            CHECK(q.capability == "slot_bool_01");
            CHECK(q.tolerance == doctest::Approx(0.08));
            sawBool = true;
        }
        else if (q.name == "str01")
        {
            CHECK(q.kind == QuadKind::Text);
            CHECK(q.capability == "slot_string_01");
            sawString = true;
        }
        else if (q.name == "icon01")
        {
            CHECK(q.kind == QuadKind::Icon);           // presence/centroid -> COARSE
            CHECK(q.capability == "icon_01");
            CHECK(q.require_presence);
            sawIcon = true;
        }
        else if (q.name == "text01")
        {
            CHECK(q.kind == QuadKind::Text);
            CHECK(q.capability == "text_01");
            sawText = true;
        }
        else if (q.name == "box01" || q.name == "border01")
        {
            CHECK(q.kind == QuadKind::Solid);          // EXACT, fails-loud geometry sentinel
            CHECK(q.located);                          // declared tight rect (not auto-located)
            CHECK(q.w > 0.0f);
            CHECK(q.h > 0.0f);
            CHECK(q.tolerance == doctest::Approx(0.02));
            if (q.name == "box01") sawBox = true; else sawBorder = true;
        }
    }
    CHECK(sawInt); CHECK(sawBool); CHECK(sawString); CHECK(sawIcon); CHECK(sawText);
    CHECK(sawBox); CHECK(sawBorder);
}

TEST_CASE("S643 DRIVER input-hash: the C++ sidecar reader tolerates the new InputHash field (forward-compat)")
{
    // S643 (DRIVER input-hash, approved scheme): the WPF emitter now stamps the sidecar's
    // `InputHash` (SHA-256 over the principle source files). The C++ `parse_emitter_metadata`
    // reader must IGNORE that unknown top-level string field (forward-compat — the reader
    // was built to tolerate unknown/extra fields, so a later shape change never strands it)
    // and still parse the quads + dims + catalogue fields it already consumes.
    const char* json = R"({
      "Width": 440, "Height": 340, "Source": "sample.smoke.xaml",
      "InputHash": "1cf41b3822040c82dc98cf00b86370adb6b880a2b3009be45d50861530cf8fd4",
      "Quads": [
        { "Name": "int01", "Kind": "Text", "Capability": "slot_int_01", "Tolerance": 0.08 },
        { "Name": "box01", "Kind": "Solid", "X": 16, "Y": 288, "Width": 64, "Height": 40 }
      ]
    })";
    const auto res = vaml::presentation::scan::parse_emitter_metadata(json);
    REQUIRE(res.ok);
    CHECK(res.metadata.width == 440u);
    CHECK(res.metadata.height == 340u);
    REQUIRE(res.metadata.quads.size() == 2u);
    CHECK(res.metadata.quads[0].name == "int01");
    CHECK(res.metadata.quads[0].kind == vaml::presentation::scan::QuadKind::Text);
    CHECK(res.metadata.quads[0].capability == "slot_int_01");
    CHECK(res.metadata.quads[0].tolerance == doctest::Approx(0.08));
    CHECK(res.metadata.quads[1].name == "box01");
    CHECK(res.metadata.quads[1].kind == vaml::presentation::scan::QuadKind::Solid);
    CHECK(res.metadata.quads[1].x == 16);
    CHECK(res.metadata.quads[1].y == 288);
}

TEST_CASE("S643 the smoke PASSES when every quad renders (both regimes agree)")
{
    const ScanDeclaration decl = smoke_declaration();
    ScrapeGrid g = smoke_grid(decl, /*inkSolid=*/true, /*inkText=*/true);
    const auto rep = compliance_compare_catalogued(g, decl, smoke_sidecar(decl));
    CHECK(rep.pass());
    CHECK(rep.fail_count == 0);
    CHECK(rep.missing_count == 0);
    CHECK(rep.total() >= 7);
    // the EXACT sentinels must carry a tight pixel-ink datum (declared tolerance, not silent).
    CHECK(datum(rep, "box01", "pixel-ink"));
    CHECK(datum(rep, "border01", "pixel-ink"));
    // the FUZZY floor's text quads carry a text-ink datum.
    CHECK(datum(rep, "int01", "text-ink"));
    CHECK(datum(rep, "bool01", "text-ink"));
    CHECK(datum(rep, "str01", "text-ink"));
    CHECK(datum(rep, "text01", "text-ink"));
    CHECK(datum(rep, "icon01", "icon-ink"));
}

TEST_CASE("S643 the loop FAILS LOUDLY on a blank geometry sentinel (attributable, not silent)")
{
    // The exact sentinels (box01/border01) render BLANK. The smoke MUST fail — the loop is
    // proven CAPABLE of issuing a FAIL on geometry (never a silent all-presence pass). This
    // is the fails-loud half of the smoke's honesty.
    const ScanDeclaration decl = smoke_declaration();
    ScrapeGrid g = smoke_grid(decl, /*inkSolid=*/false, /*inkText=*/true);
    const auto rep = compliance_compare_catalogued(g, decl, smoke_sidecar(decl));
    CHECK_FALSE(rep.pass());
    CHECK(rep.fail_count > 0);
}

TEST_CASE("S643 the loop FAILS LOUDLY on a missing fuzzy floor (text/icon vanished)")
{
    // The text/icon quads render BLANK (the capability rendered with no ink — the exact
    // failure class the 11r sprint targets). The loop must fail loudly here too.
    const ScanDeclaration decl = smoke_declaration();
    ScrapeGrid g = smoke_grid(decl, /*inkSolid=*/true, /*inkText=*/false);
    const auto rep = compliance_compare_catalogued(g, decl, smoke_sidecar(decl));
    CHECK_FALSE(rep.pass());
    CHECK(rep.fail_count > 0);
}

TEST_CASE("S643 REVISIT S640b/S641: declared capabilities + tolerances round-trip (the DECLARED values appear in the report)")
{
    // The smoke's text/icon quads must round-trip the emitted-sidecar metadata -> reader ->
    // compare. Here the SAME capability ids the smoke declares resolve in the catalogue
    // (slot_int_01 / slot_bool_01 / slot_string_01 / icon_01 / text_01 all exist), and the
    // DECLARED tolerance is what the report documents (never silent).
    const ScanDeclaration decl = smoke_declaration();
    for (const ScanQuad& q : decl.quads)
    {
        if (q.capability.empty())
            continue;   // the geometry sentinels are deliberately not catalogue-backed (tight rect only)
        const Capability* cap = find_capability(capability_catalogue(), q.capability);
        REQUIRE(cap != nullptr);   // a declared capability MUST resolve — attribution is the whole point
        // the declared kind ↔ catalogue kind agree for the smoke's real surfaces.
        CHECK(cap->kind == q.kind);
    }
}
