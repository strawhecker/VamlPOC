// S641 — ComplianceCompare_s641_test.cpp (compliance_compare + per-datum detail).
//
// Sprint 11r (S641): the compare seam `compliance_compare(actualScan,
// expectedDecl)` -> ScanReport with PER-DATUM PASS/FAIL(detail) — never a bare
// global bit. This suite drives the pure scrape/compare decision layer with a
// SYNTHETIC ImageBuffer + element grid (the S162/S200 precedent — headless on
// BOTH CI lanes, no Vulkan runtime). The REAL GPU render that fills the buffer
// is S640a's RenderTarget [both apps, not CI].
//
// Covers: solid-at-rect (element present + ink), text auto-locate (ink + band),
// icon (presence + ink + centroid), Missing (absent element -> missing datum,
// never a silent pass), full-missing-content (blank region where ink expected),
// and the per-quad tolerance merge (declared wins; sidecar kind -> default).

#include <vaml/presentation/scan/Scanner.h>
#include <vaml/presentation/scan/ScanDeclaration.h>

#include "doctest/doctest.h"

#include <string>
#include <vector>
#include <cstdint>

using namespace vaml::presentation::scan;

namespace {

// Builds a ScrapeGrid with a solid inked rectangle at the given frame rect
// (all pixels inside the rect are opaque non-black).
ScrapeGrid make_grid(std::uint32_t w, std::uint32_t h,
                     std::vector<ScrapeGrid::ElementSample> elems)
{
    ScrapeGrid g;
    g.width = w;
    g.height = h;
    g.bgra.assign(static_cast<std::size_t>(w) * h * 4u, 0u);
    for (const auto& e : elems)
    {
        g.elements.push_back(e);
        // ink the element's rect (opaque dark-grey so "ink" is detected)
        const int x0 = static_cast<int>(e.x);
        const int y0 = static_cast<int>(e.y);
        const int x1 = static_cast<int>(std::min((double)w, (double)(e.x + e.w)));
        const int y1 = static_cast<int>(std::min((double)h, (double)(e.y + e.h)));
        for (int yy = y0; yy < y1; ++yy)
        {
            for (int xx = x0; xx < x1; ++xx)
            {
                std::size_t off = (static_cast<std::size_t>(yy) * w + static_cast<std::size_t>(xx)) * 4u;
                g.bgra[off + 0] = 90u; // B
                g.bgra[off + 1] = 90u; // G
                g.bgra[off + 2] = 90u; // R
                g.bgra[off + 3] = 255u; // A
            }
        }
    }
    return g;
}

ScanDeclaration decl_with(ScanQuad q)
{
    ScanDeclaration d;
    d.width = 100u;
    d.height = 80u;
    d.quads.push_back(std::move(q));
    return d;
}

} // namespace

TEST_CASE("S641 compliance_compare solid-at-rect PASSES with element present + ink")
{
    ScrapeGrid g = make_grid(100u, 80u, { {"img", 10.0f, 10.0f, 40.0f, 40.0f} });
    ScanQuad q;
    q.name = "img"; q.kind = QuadKind::Solid; q.located = true;
    q.x = 8.0f; q.y = 8.0f; q.w = 44.0f; q.h = 44.0f;
    const auto rep = compliance_compare(g, decl_with(q));
    CHECK(rep.pass());
    CHECK(rep.fail_count == 0);
    CHECK(rep.missing_count == 0);
    // the element-present datum passes
    bool sawPresent = false, sawInk = false;
    for (const auto& d : rep.data)
    {
        if (d.what == "element-present") { sawPresent = true; CHECK(d.status == DatumStatus::Pass); }
        if (d.what == "pixel-ink")       { sawInk = true; CHECK(d.status == DatumStatus::Pass); }
    }
    CHECK(sawPresent);
    CHECK(sawInk);
}

TEST_CASE("S641 compliance_compare reports a MISSING datum (never a silent pass)")
{
    // A declared solid quad whose element is ABSENT from the scrape.
    ScrapeGrid g = make_grid(100u, 80u, { });
    ScanQuad q;
    q.name = "ghost"; q.kind = QuadKind::Solid; q.located = true;
    q.x = 0.0f; q.y = 0.0f; q.w = 10.0f; q.h = 10.0f;
    const auto rep = compliance_compare(g, decl_with(q));
    CHECK_FALSE(rep.pass());
    CHECK(rep.missing_count >= 1);
    bool sawMissing = false;
    for (const auto& d : rep.data)
        if (d.what == "element-present" && d.status == DatumStatus::Missing)
            sawMissing = true;
    CHECK(sawMissing);
}

TEST_CASE("S641 compliance_compare text auto-locate needs readable ink (full-missing-content)")
{
    // A text quad auto-located to the element, but the scrape has NO ink at it
    // (the element's rect was NOT painted) -> Fail datum named text-ink.
    ScrapeGrid g;
    g.width = 100u; g.height = 80u;
    g.bgra.assign(100u * 80u * 4u, 0u);   // entirely blank
    g.elements.push_back({ "title", 20.0f, 20.0f, 60.0f, 20.0f });
    ScanQuad q;
    q.name = "title"; q.kind = QuadKind::Text; q.located = false; q.band = 8;
    const auto rep = compliance_compare(g, decl_with(q));
    CHECK_FALSE(rep.pass());
    bool sawInkFail = false;
    for (const auto& d : rep.data)
        if (d.what == "text-ink" && d.status == DatumStatus::Fail)
            sawInkFail = true;
    CHECK(sawInkFail);
}

TEST_CASE("S641 compliance_compare icon presence + ink + centroid pass")
{
    ScrapeGrid g = make_grid(100u, 80u, { {"ok", 30.0f, 30.0f, 20.0f, 20.0f} });
    ScanQuad q;
    q.name = "ok"; q.kind = QuadKind::Icon; q.located = false;
    q.require_presence = true; q.require_centroid = true;
    const auto rep = compliance_compare(g, decl_with(q));
    CHECK(rep.pass());
    bool sawPresence = false, sawCentroid = false;
    for (const auto& d : rep.data)
    {
        if (d.what == "icon-presence") { sawPresence = true; CHECK(d.status == DatumStatus::Pass); }
        if (d.what == "icon-centroid") { sawCentroid = true; CHECK(d.status == DatumStatus::Pass); }
    }
    CHECK(sawPresence);
    CHECK(sawCentroid);
}

TEST_CASE("S641 compliance_compare uses the DECLARED per-quad tolerance (declared wins)")
{
    ScrapeGrid g = make_grid(100u, 80u, { {"img", 10.0f, 10.0f, 40.0f, 40.0f} });
    ScanQuad q;
    q.name = "img"; q.kind = QuadKind::Icon; q.located = true;
    q.x = 8.0f; q.y = 8.0f; q.w = 44.0f; q.h = 44.0f;
    q.has_tolerance = true; q.tolerance = 0.35;   // an explicit override
    const auto rep = compliance_compare(g, decl_with(q));
    REQUIRE_FALSE(rep.data.empty());
    // the merged tolerance field should carry the declared 0.35 (the scaffold)
    bool sawTol = false;
    for (const auto& d : rep.data)
        if (d.what == "element-present" && std::abs(d.tolerance - 0.35) < 1e-6)
            sawTol = true;
    CHECK(sawTol);
}

TEST_CASE("S641 compliance_compare merges the emitter-sidecar kind -> default tolerance")
{
    // The declaration carries a quad (default Solid, no tolerance); the sidecar
    // says it is an ICON -> the tolerance scaffold merges the Icon default (0.25).
    ScrapeGrid g = make_grid(100u, 80u, { {"ok", 10.0f, 10.0f, 40.0f, 40.0f} });
    ScanQuad q;
    q.name = "ok"; q.kind = QuadKind::Solid; q.located = true;
    q.x = 8.0f; q.y = 8.0f; q.w = 44.0f; q.h = 44.0f;
    std::vector<std::pair<std::string, QuadKind>> sidecar = { {"ok", QuadKind::Icon} };
    const auto rep = compliance_compare(g, decl_with(q), sidecar);
    bool sawTol = false;
    for (const auto& d : rep.data)
        if (d.what == "element-present" && std::abs(d.tolerance - 0.25) < 1e-6)
            sawTol = true;
    CHECK(sawTol);
}

TEST_CASE("S641 compliance_compare build QuadGrid samples ink + centroid correctly")
{
    ScrapeGrid g = make_grid(100u, 80u, { {"box", 10.0f, 10.0f, 40.0f, 40.0f} });
    const QuadGrid grid = sample_quad(g, 0.0f, 0.0f, 100.0f, 80.0f);
    CHECK(grid.has_ink);
    CHECK(grid.ink_pixels >= 40u * 40u);
    // centroid should be near the box center (25,25)... actually box is 10..50 ->
    // centroid ~ (30,30)
    CHECK(grid.centroid_x > 25.0f);
    CHECK(grid.centroid_x < 35.0f);
    CHECK(grid.centroid_y > 25.0f);
    CHECK(grid.centroid_y < 35.0f);
}

TEST_CASE("S641 sample_quad reports NO ink on a blank region")
{
    ScrapeGrid g;
    g.width = 100u; g.height = 80u;
    g.bgra.assign(100u * 80u * 4u, 0u);
    const QuadGrid grid = sample_quad(g, 0.0f, 0.0f, 100.0f, 80.0f);
    CHECK_FALSE(grid.has_ink);
    CHECK(grid.ink_pixels == 0u);
}
