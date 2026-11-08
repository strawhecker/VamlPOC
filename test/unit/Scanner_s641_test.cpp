// S641 — Scanner_s641_test.cpp (ScrapeGrid + compliance_compare end-to-end on a real tree).
//
// Sprint 11r (S641): the scanner makes the compare LIVE — render an authored
// `.xaml` through the REAL retained render path -> S640a `ImageBuffer` -> SCRAPE
// the rendered UI (element/draw-item presence + coordinates + pixel/color at
// declared locations) -> COMPARE to the declared outcome. This suite proves the
// SCRAPE half on a REAL arranged core tree (vaml::UIElement) + the derived
// ScrapeGrid, on BOTH CI lanes (headless — no Vulkan runtime). The REAL GPU
// render that fills the pixel buffer is S640a's RenderTarget [both apps, not CI].
//
// scrape_elements_from_tree: walks an arranged core tree and emits the
// element-presence + arranged-rect samples the compare reads — the "element
// presence + coordinates" ask from the roadmap. Proven here against a real
// UIElement tree (arrange via the S123/S124 Measure/Arrange surface).

#include <vaml/presentation/scan/Scanner.h>
#include <vaml/presentation/scan/ScanDeclaration.h>

#include <vaml/core/UIElement.h>

#include "doctest/doctest.h"

#include <string>
#include <vector>
#include <cstdint>

using namespace vaml::presentation::scan;
using namespace vaml;

namespace {

// Walks an arranged core tree and emits element samples (presence + final_rect),
// naming each visited element from a caller-supplied name lookup (the core
// element has no `set_name` — the real tester maps x:Name via the reader's name
// registry; here a local map keys by pointer). PURE + headless (a real
// vaml::UIElement tree, no GPU).
using NameMap = std::vector<std::pair<const vaml::UIElement*, std::string>>;

const char* lookup_name(const NameMap& names, const vaml::UIElement& el)
{
    for (const auto& kv : names)
        if (kv.first == &el)
            return kv.second.c_str();
    return "";
}

void scrape_elements_from_tree(UIElement& root, const NameMap& names, ScrapeGrid& out)
{
    root.walk([&](UIElement& el) {
        const vaml::Rect r = el.final_rect();
        ScrapeGrid::ElementSample s;
        s.name = lookup_name(names, el);
        s.x = r.x; s.y = r.y; s.w = r.width; s.h = r.height;
        out.elements.push_back(std::move(s));
    });
}

} // namespace

TEST_CASE("S641 scrape of an arranged core tree yields element presence + coordinates")
{
    // Build a small real tree: a root (named 'root') with one child ('img') at
    // a known arranged rect.
    UIElement root;
    UIElement* img = root.create_child<UIElement>();

    // Arrange the tree at a 100x80 frame.
    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 80.0f });

    // The child gets laid out into the root's full final slot; force a known
    // rect via Arrange on the child.
    img->Arrange(vaml::Rect{ 10.0f, 20.0f, 40.0f, 30.0f });

    NameMap names = { { &root, "root" }, { img, "img" } };

    ScrapeGrid g;
    g.width = 100u; g.height = 80u;
    g.bgra.assign(static_cast<std::size_t>(100u) * 80u * 4u, 0u);  // blank frame
    scrape_elements_from_tree(root, names, g);

    // Root is present at the full frame.
    bool sawRoot = false, sawImg = false;
    for (const auto& e : g.elements)
    {
        if (e.name == "root")
        {
            sawRoot = true;
            CHECK(e.x == 0.0f);
            CHECK(e.y == 0.0f);
            CHECK(e.w >= 100.0f - 1.0f);
            CHECK(e.h >= 80.0f - 1.0f);
        }
        if (e.name == "img")
        {
            sawImg = true;
            CHECK(e.x == doctest::Approx(10.0f));
            CHECK(e.y == doctest::Approx(20.0f));
            CHECK(e.w == doctest::Approx(40.0f));
            CHECK(e.h == doctest::Approx(30.0f));
        }
    }
    CHECK(sawRoot);
    CHECK(sawImg);
}

TEST_CASE("S641 scrape + compare end-to-end on a real arranged tree (solid PASS)")
{
    UIElement root;
    UIElement* img = root.create_child<UIElement>();
    root.Measure(vaml::Size{ 100.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 100.0f, 80.0f });
    img->Arrange(vaml::Rect{ 10.0f, 10.0f, 40.0f, 40.0f });

    NameMap names = { { &root, "root" }, { img, "img" } };

    ScrapeGrid g;
    g.width = 100u; g.height = 80u;
    g.bgra.assign(static_cast<std::size_t>(100u) * 80u * 4u, 0u);  // blank frame, then ink
    scrape_elements_from_tree(root, names, g);
    // ink the img element's rect in the pixel buffer (the rendered pixels).
    for (int yy = 10; yy < 50; ++yy)
        for (int xx = 10; xx < 50; ++xx)
        {
            std::size_t off = (static_cast<std::size_t>(yy) * 100u + static_cast<std::size_t>(xx)) * 4u;
            g.bgra[off + 0] = 90u; g.bgra[off + 1] = 90u; g.bgra[off + 2] = 90u; g.bgra[off + 3] = 255u;
        }

    ScanDeclaration d;
    d.width = 100u; d.height = 80u;
    ScanQuad q;
    q.name = "img"; q.kind = QuadKind::Solid; q.located = true;
    q.x = 8.0f; q.y = 8.0f; q.w = 44.0f; q.h = 44.0f;
    d.quads.push_back(q);

    const auto rep = compliance_compare(g, d);
    CHECK(rep.pass());
}
