// S642 — CapabilityCompare_s642_test.cpp (the catalogue-driven compare overload +
// the emitter-sidecar Capability/Tolerance read).
//
// Sprint 11r (S642 — REVISIT S640b/S641 metadata fine-tune, PROVISIONAL): the
// catalogue-driven compare resolves each quad's effective tolerance through the
// capability catalogue (the SCAN DECLARATION's declared `tolerance` still wins;
// else the emitter sidecar's capability-tagged catalogue tolerance; else the built-in
// kind default). This suite proves the sync-token tolerance info flows
// emitter-sidecar -> reader -> catalogue -> compare. PURE + headless (both CI lanes).

#include <vaml/presentation/scan/Scanner.h>
#include <vaml/presentation/scan/ScanDeclaration.h>
#include <vaml/presentation/scan/EmitterMetadata.h>
#include <vaml/presentation/scan/CapabilityCatalogue.h>

#include "doctest/doctest.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace vaml::presentation::scan;

namespace {

// Builds a ScrapeGrid with a solid inked rectangle at the given frame rect.
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
        const int x0 = static_cast<int>(e.x);
        const int y0 = static_cast<int>(e.y);
        const int x1 = static_cast<int>(std::min((double)w, (double)(e.x + e.w)));
        const int y1 = static_cast<int>(std::min((double)h, (double)(e.y + e.h)));
        for (int yy = y0; yy < y1; ++yy)
            for (int xx = x0; xx < x1; ++xx)
            {
                std::size_t off = (static_cast<std::size_t>(yy) * w + static_cast<std::size_t>(xx)) * 4u;
                g.bgra[off + 0] = 90u;
                g.bgra[off + 1] = 90u;
                g.bgra[off + 2] = 90u;
                g.bgra[off + 3] = 255u;
            }
    }
    return g;
}

bool any_tolerance(const ScanReport& rep, const std::string& what, double tol)
{
    for (const auto& d : rep.data)
        if (d.what == what && std::abs(d.tolerance - tol) < 1e-6)
            return true;
    return false;
}

} // namespace

TEST_CASE("S642 the emitter sidecar Capability/Tolerance fields parse forward-compatibly")
{
    // The exact new provisional sidecar shape the emitter deposits (S642): quads
    // carry a Capability id + Tolerance plus the prior Kind.
    const std::string json = R"({
  "Width": 440,
  "Height": 240,
  "Source": "sample.image.xaml",
  "Quads": [
    { "Name": "SampleText", "X": 16, "Y": 96, "Width": 120, "Height": 40,
      "Kind": "Text", "Capability": "text_01", "Tolerance": 0.08 },
    { "Name": "SampleIcon", "X": 16, "Y": 16, "Width": 64, "Height": 64,
      "Kind": "Icon", "Capability": "icon_01", "Tolerance": 0.25 }
  ]
})";
    const auto r = parse_emitter_metadata(json);
    REQUIRE(r.ok);
    REQUIRE(r.metadata.quads.size() == 2u);
    CHECK(r.metadata.quads[0].capability == "text_01");
    CHECK(r.metadata.quads[0].has_tolerance);
    CHECK(r.metadata.quads[0].tolerance == doctest::Approx(0.08));
    CHECK(r.metadata.quads[0].kind == QuadKind::Text);
    CHECK(r.metadata.quads[1].capability == "icon_01");
    CHECK(r.metadata.quads[1].has_tolerance);
    CHECK(r.metadata.quads[1].tolerance == doctest::Approx(0.25));
    CHECK(r.metadata.quads[1].kind == QuadKind::Icon);
}

TEST_CASE("S642 the S641 forward-compat contract holds (no Capability field = older shape still parses)")
{
    const std::string json = R"({ "Width": 10, "Height": 10, "Quads": [
      { "Name": "a", "X": 0, "Y": 0, "Width": 2, "Height": 2, "Kind": "Text" }
    ]})";
    const auto r = parse_emitter_metadata(json);
    REQUIRE(r.ok);
    REQUIRE(r.metadata.quads.size() == 1u);
    CHECK(r.metadata.quads[0].name == "a");
    CHECK(r.metadata.quads[0].capability.empty());
    CHECK_FALSE(r.metadata.quads[0].has_tolerance);
    CHECK(r.metadata.quads[0].kind == QuadKind::Text);
}

TEST_CASE("S642 catalogue-driven compare: sidecar icon capability -> loose tolerance merge")
{
    // A quad declared SOLID (no tolerance); the sidecar tags it ICON (icon_01, loose
    // 0.25) + a catalogued tolerance. The catalogue-driven compare must apply the
    // icon tolerance via the capability catalogue.
    ScrapeGrid g = make_grid(100u, 80u, { {"ok", 10.0f, 10.0f, 40.0f, 40.0f} });
    ScanDeclaration decl;
    decl.width = 100u; decl.height = 80u;
    ScanQuad q;
    q.name = "ok"; q.kind = QuadKind::Solid; q.located = true;
    q.x = 8.0f; q.y = 8.0f; q.w = 44.0f; q.h = 44.0f;
    decl.quads.push_back(q);

    EmitterMetadata sidecar;
    MetadataQuad mq;
    mq.name = "ok"; mq.kind = QuadKind::Icon;
    mq.capability = "icon_01";
    mq.has_tolerance = true; mq.tolerance = 0.25;
    sidecar.quads.push_back(mq);

    const auto rep = compliance_compare_catalogued(g, decl, sidecar);
    CHECK(rep.pass());
    // the merged tolerance should have been lifted to the icon 0.25 (not the solid 0.02)
    CHECK(any_tolerance(rep, "element-present", 0.25));
}

TEST_CASE("S642 declared per-quad tolerance still WINS over the catalogue (the merge order)")
{
    // The SCAN DECLARATION's declared tolerance is the PRIMARY oracle — it overrides
    // the catalogue sidecar.
    ScrapeGrid g = make_grid(100u, 80u, { {"ok", 10.0f, 10.0f, 40.0f, 40.0f} });
    ScanDeclaration decl;
    decl.width = 100u; decl.height = 80u;
    ScanQuad q;
    q.name = "ok"; q.kind = QuadKind::Icon; q.located = true;
    q.x = 8.0f; q.y = 8.0f; q.w = 44.0f; q.h = 44.0f;
    q.has_tolerance = true; q.tolerance = 0.50;   // explicit declaration override
    decl.quads.push_back(q);

    EmitterMetadata sidecar;
    MetadataQuad mq;
    mq.name = "ok"; mq.kind = QuadKind::Icon;
    mq.capability = "icon_01"; mq.tolerance = 0.25;
    sidecar.quads.push_back(mq);

    const auto rep = compliance_compare_catalogued(g, decl, sidecar);
    CHECK(rep.pass());
    CHECK(any_tolerance(rep, "element-present", 0.50));
}

TEST_CASE("S642 catalogue-driven compare: a catalogued string capability -> text tolerance")
{
    ScrapeGrid g = make_grid(100u, 80u, { {"title", 20.0f, 20.0f, 60.0f, 20.0f} });
    ScanDeclaration decl;
    decl.width = 100u; decl.height = 80u;
    ScanQuad q;
    q.name = "title"; q.kind = QuadKind::Text; q.located = false;
    decl.quads.push_back(q);

    EmitterMetadata sidecar;
    MetadataQuad mq;
    mq.name = "title"; mq.kind = QuadKind::Text;
    mq.capability = "slot_string_01";   // a catalogued string slot -> text tolerance 0.08
    sidecar.quads.push_back(mq);

    const auto rep = compliance_compare_catalogued(g, decl, sidecar);
    CHECK(rep.pass());
    // the string capability's catalogue tolerance (0.08) is applied
    CHECK(any_tolerance(rep, "text-ink", 0.08));
}

TEST_CASE("S642 catalogued tolerance resolve: catalogue wins, else the kind default")
{
    // catalogue_tolerance_for on a catalogued capability returns its declared tolerance.
    const Capability* ic = find_capability(capability_catalogue(), "icon_01");
    REQUIRE(ic != nullptr);
    CHECK(catalogue_tolerance_for(*ic) == doctest::Approx(0.25));
    // A catalogued text capability -> 0.08.
    const Capability* tx = find_capability(capability_catalogue(), "text_01");
    REQUIRE(tx != nullptr);
    CHECK(tx->kind == QuadKind::Text);
    CHECK(tx->tolerance == doctest::Approx(0.08));
}

TEST_CASE("S642 the committed sidecar fixture is catalogued (the sync-token fixture half)")
{
    // The committed fixture the WPF-emitter side commits to (test/fixtures/s642_emitter_sidecar.json)
    // MUST agree with the embedded catalogue: every capability id the fixture deposits
    // resolves to a catalogue row with a matching kind + tolerance — so a WPF-side drift
    // is ENFORCED by this C++ headless test (the other half of the sync token), never
    // silently assumed green.
    std::ifstream fs(VAML_TEST_FIXTURES_DIR "/s642_emitter_sidecar.json");
    REQUIRE(fs.good());
    std::ostringstream ss;
    ss << fs.rdbuf();
    const auto r = parse_emitter_metadata(ss.str());
    REQUIRE(r.ok);
    REQUIRE(r.metadata.quads.size() == 3u);

    // every quad carries a capability id + a tolerance, and that id resolves in the
    // catalogue with the same kind + tolerance.
    for (const auto& q : r.metadata.quads)
    {
        INFO("quad " << q.name << " capability=" << q.capability);
        CHECK_FALSE(q.capability.empty());
        CHECK(q.has_tolerance);
        CHECK(q.tolerance > 0.0);

        const Capability* cap = find_capability(capability_catalogue(), q.capability);
        REQUIRE(cap != nullptr);
        CHECK(cap->kind == q.kind);
        CHECK(catalogue_tolerance_for(*cap) == doctest::Approx(q.tolerance));
    }
}
