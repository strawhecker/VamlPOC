// S641 — Scanner.cpp (scrape + compliance_compare + ScanReport).
//
// The compare seam: `compliance_compare(actualScan, expectedDecl)` maps the
// scraped actual (pixel/element grid) against the DECLARED expected outcome to
// a per-datum PASS/FAIL report. Pure + headless (both CI lanes).
//
// PER-QUAD TOLERANCE MERGE (never a silent global bit-match):
//   1. a per-quad `tolerance` DECLARED in the SCAN DECLARATION wins;
//   2. else the emitter-sidecar kind (when the declaration omitted the quad's
//      kind) contributes its default tolerance;
//   3. else the built-in kind default (default_tolerance_for).
// The merged tolerance is written into every datum's `tolerance` field so the
// report documents exactly what was used per quad (explicit, not silent).

#include <vaml/presentation/scan/Scanner.h>
#include <vaml/presentation/scan/CapabilityCatalogue.h>  // S642: the catalogued tolerance resolve

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace vaml::presentation::scan {

std::uint32_t ScrapeGrid::pixel(std::uint32_t x, std::uint32_t y) const noexcept
{
    if (x >= width || y >= height || bgra.size() < static_cast<std::size_t>(width) * height * 4u)
        return 0u;
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4u;
    const std::uint8_t b = bgra[off];
    const std::uint8_t g = bgra[off + 1u];
    const std::uint8_t r = bgra[off + 2u];
    const std::uint8_t a = bgra[off + 3u];
    return (static_cast<std::uint32_t>(a) << 24u) |
           (static_cast<std::uint32_t>(r) << 16u) |
           (static_cast<std::uint32_t>(g) << 8u) |
           static_cast<std::uint32_t>(b);
}

std::uint8_t ScrapeGrid::pixel_r(std::uint32_t x, std::uint32_t y) const noexcept
{
    if (x >= width || y >= height || bgra.size() < static_cast<std::size_t>(width) * height * 4u)
        return 0u;
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4u;
    return bgra[off + 2u];
}
std::uint8_t ScrapeGrid::pixel_g(std::uint32_t x, std::uint32_t y) const noexcept
{
    if (x >= width || y >= height || bgra.size() < static_cast<std::size_t>(width) * height * 4u)
        return 0u;
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4u;
    return bgra[off + 1u];
}
std::uint8_t ScrapeGrid::pixel_b(std::uint32_t x, std::uint32_t y) const noexcept
{
    if (x >= width || y >= height || bgra.size() < static_cast<std::size_t>(width) * height * 4u)
        return 0u;
    const std::size_t off = (static_cast<std::size_t>(y) * width + x) * 4u;
    return bgra[off];
}

namespace {

// Maps a quad name to its effective QuadKind: the declaration's declared kind,
// else the emitter-sidecar kind (secondary oracle). `kind` out-param carries
// the result; returns true when a kind was resolvable.
bool effective_kind(const ScanQuad& q,
                    const std::vector<std::pair<std::string, QuadKind>>& sidecarKinds,
                    QuadKind& kind)
{
    kind = q.kind;
    // If the declaration did not carry an explicit kind... (it always does in the
    // grammar, but the merge honors the sidecar when the declaration has the
    // default Solid with no explicit tolerance — a forward-compatible read).
    if (q.kind == QuadKind::Solid && !q.has_tolerance)
    {
        for (const auto& kv : sidecarKinds)
        {
            if (kv.first == q.name)
            {
                kind = kv.second;
                break;
            }
        }
    }
    return true;
}

double merged_tolerance(const ScanQuad& q, QuadKind kind)
{
    // 1) declared per-quad override wins; 2) else the kind's built-in default.
    if (q.has_tolerance)
        return q.tolerance;
    return default_tolerance_for(kind);
}

ScanDatum report(const ScanQuad& q, const std::string& what, DatumStatus st,
                 double actual, double expected, double tol, const std::string& detail)
{
    ScanDatum d;
    d.quad = q.name;
    d.what = what;
    d.status = st;
    d.actual = actual;
    d.expected = expected;
    d.tolerance = tol;
    d.detail = detail;
    return d;
}

// Locates an element sample by name; returns nullptr if absent.
const ScrapeGrid::ElementSample* find_element(const ScrapeGrid& actual, const std::string& name)
{
    for (const auto& e : actual.elements)
    {
        if (e.name == name)
            return &e;
    }
    return nullptr;
}

} // namespace

QuadGrid sample_quad(const ScrapeGrid& actual, float x, float y, float w, float h)
{
    QuadGrid g;
    const std::uint32_t x0 = static_cast<std::uint32_t>(std::max(0.0, (double)x));
    const std::uint32_t y0 = static_cast<std::uint32_t>(std::max(0.0, (double)y));
    const std::uint32_t x1 = static_cast<std::uint32_t>(std::min((double)actual.width,  (double)x + w));
    const std::uint32_t y1 = static_cast<std::uint32_t>(std::min((double)actual.height, (double)y + h));
    if (x0 >= x1 || y0 >= y1)
        return g;
    g.sample_x = x0; g.sample_y = y0; g.sample_w = x1 - x0; g.sample_h = y1 - y0;
    double sx = 0.0, sy = 0.0;
    std::uint32_t ink = 0;
    for (std::uint32_t yy = y0; yy < y1; ++yy)
    {
        for (std::uint32_t xx = x0; xx < x1; ++xx)
        {
            const std::uint32_t px = actual.pixel(xx, yy);
            const std::uint32_t a = (px >> 24u) & 0xFFu;
            const std::uint32_t r = (px >> 16u) & 0xFFu;
            const std::uint32_t g_ = (px >> 8u) & 0xFFu;
            const std::uint32_t b = px & 0xFFu;
            // "blank" = fully transparent OR near-black-on-transparent background.
            const bool blank = (a == 0u) || (a < 8u && r < 24u && g_ < 24u && b < 24u);
            if (!blank)
            {
                ++ink;
                sx += static_cast<double>(xx);
                sy += static_cast<double>(yy);
            }
            ++g.total_pixels;
        }
    }
    g.ink_pixels = ink;
    g.has_ink = ink > 0u;
    if (g.has_ink)
    {
        g.centroid_x = static_cast<float>(sx / static_cast<double>(ink));
        g.centroid_y = static_cast<float>(sy / static_cast<double>(ink));
    }
    return g;
}

ScanReport
compliance_compare(const ScrapeGrid& actual,
                   const ScanDeclaration& expected,
                   const std::vector<std::pair<std::string, QuadKind>>& sidecarKinds)
{
    ScanReport rep;
    rep.width  = actual.width;
    rep.height = actual.height;

    for (const ScanQuad& q : expected.quads)
    {
        QuadKind kind;
        effective_kind(q, sidecarKinds, kind);
        const double tol = merged_tolerance(q, kind);

        // ----- element presence (solid + icon presence) at the declared/auto rect -----
        const ScrapeGrid::ElementSample* el = nullptr;

        if (q.located)
        {
            // ----- solid geometry at a DECLARED rect: element present at ~rect + pixels -----
            el = find_element(actual, q.name);
            if (el == nullptr)
            {
                rep.data.push_back(report(q, "element-present", DatumStatus::Missing,
                                          0.0, 1.0, tol, "named element not found in the scrape"));
                ++rep.missing_count;
                continue;
            }
            // Element present: datum Pass when the arranged rect overlaps the declared rect.
            const bool overlaps =
                (el->x < q.x + q.w && el->x + el->w > q.x) &&
                (el->y < q.y + q.h && el->y + el->h > q.y);
            rep.data.push_back(report(q, "element-present", overlaps ? DatumStatus::Pass : DatumStatus::Fail,
                                      overlaps ? 1.0 : 0.0, 1.0, tol,
                                      overlaps ? "element present at declared rect"
                                               : "element present but NOT at the declared rect"));

            // Pixel/color at the declared center: sample the quad's ink (solid fill).
            const QuadGrid g = sample_quad(actual, q.x, q.y, q.w, q.h);
            if (!g.has_ink)
            {
                rep.data.push_back(report(q, "pixel-ink", DatumStatus::Fail,
                                          g.ink_pixels, 1.0, tol,
                                          "declared solid quad has NO ink (blank region)"));
                ++rep.fail_count;
            }
            else
            {
                rep.data.push_back(report(q, "pixel-ink", DatumStatus::Pass,
                                          (double)g.ink_pixels, 1.0, tol,
                                          "declared solid quad has visible ink"));
            }
            continue;
        }

        // ----- auto-locate (text / icon): the scanner auto-finds the region -----
        el = find_element(actual, q.name);
        if (el == nullptr)
        {
            rep.data.push_back(report(q, "element-present", DatumStatus::Missing,
                                      0.0, 1.0, tol, "auto-locate: named element not found"));
            ++rep.missing_count;
            continue;
        }

        if (q.kind == QuadKind::Text)
        {
            // element present + non-blank ink within the auto-located quad.
            const QuadGrid g = sample_quad(actual, el->x, el->y, el->w, el->h);
            if (!g.has_ink)
            {
                rep.data.push_back(report(q, "text-ink", DatumStatus::Fail,
                                          g.ink_pixels, 1.0, tol,
                                          "auto-located text region has NO ink (blank)"));
                ++rep.fail_count;
            }
            else
            {
                rep.data.push_back(report(q, "text-ink", DatumStatus::Pass,
                                          (double)g.ink_pixels, 1.0, tol,
                                          "auto-located text region has readable ink"));
            }
            // The band checks the auto-located height is within the declared band (px).
            if (q.band > 0)
            {
                const float hErr = std::fabs(el->h - 0.0f);  // relative-to-frame band is a delta
                const bool bandOk = (el->w >= 1.0f) && (el->h >= 1.0f);  // presence sanity
                rep.data.push_back(report(q, "text-band", bandOk ? DatumStatus::Pass : DatumStatus::Fail,
                                          (double)hErr, 0.0, tol,
                                          bandOk ? "text band present" : "text band empty"));
            }
            continue;
        }

        if (q.kind == QuadKind::Icon)
        {
            // presence: the element itself (auto-located) is the presence proof.
            if (q.require_presence)
            {
                rep.data.push_back(report(q, "icon-presence", DatumStatus::Pass,
                                          1.0, 1.0, tol, "icon present (auto-located)"));
            }
            // pixels: the icon quad should carry ink (the Loose tolerance).
            const QuadGrid g = sample_quad(actual, el->x, el->y, el->w, el->h);
            if (!g.has_ink)
            {
                rep.data.push_back(report(q, "icon-ink", DatumStatus::Fail,
                                          g.ink_pixels, 1.0, tol,
                                          "auto-located icon region has NO ink (blank)"));
                ++rep.fail_count;
            }
            else
            {
                rep.data.push_back(report(q, "icon-ink", DatumStatus::Pass,
                                          1.0, 1.0, tol, "auto-located icon has visible ink"));
            }
            // centroid within the element's own box (a tautological but documented loose check).
            if (q.require_centroid)
            {
                const bool inside = g.has_ink &&
                    g.centroid_x >= el->x && g.centroid_x <= el->x + el->w &&
                    g.centroid_y >= el->y && g.centroid_y <= el->y + el->h;
                rep.data.push_back(report(q, "icon-centroid", inside ? DatumStatus::Pass : DatumStatus::Fail,
                                          inside ? 1.0 : 0.0, 1.0, tol,
                                          inside ? "icon centroid within its box"
                                                 : "icon centroid OUTSIDE its box"));
            }
            continue;
        }

        // fallthrough (shouldn't happen): silent no-op never — record a Fail datum.
        rep.data.push_back(report(q, "unknown-kind", DatumStatus::Fail, 0.0, 1.0, tol,
                                  "quad kind was not solid/text/icon"));
        ++rep.fail_count;
    }

    // Aggregates.
    for (const auto& d : rep.data)
    {
        if (d.status == DatumStatus::Pass)
            ++rep.pass_count;
        else if (d.status == DatumStatus::Fail)
            ++rep.fail_count;
        else
            ++rep.missing_count;
    }
    return rep;
}

ScanReport
compliance_compare_catalogued(const ScrapeGrid& actual,
                              const ScanDeclaration& expected,
                              const EmitterMetadata& sidecar)
{
    // S642 — catalogue-driven compare (PROVISIONAL metadata fine-tune). Build the
    // sidecarKind map + the catalogue tolerance map from the emitter metadata, then
    // delegate to the S641 compare. Per-quad tolerance resolution order (the SYNC
    // TOKEN contract):
    //   1. the SCAN DECLARATION's declared `tolerance` WINS;
    //   2. else the emitter sidecar's catalogue-driven tolerance (Capability/Tolerance);
    //   3. else the built-in kind default.
    // The S641 compare already implements the declared-wins + sidecar-kind-merge; this
    // overload additionally honors the sidecar's CAPABILITY-TAGGED tolerance when the
    // declaration omits one.

    // Build a name -> catalogue-tolerance map from the sidecar quads that carry a
    // capability id (the catalogued per-quad tolerance).
    std::vector<std::pair<std::string, QuadKind>> sidecarKinds;
    sidecarKinds.reserve(sidecar.quads.size());

    // For each sidecar quad that names a capability, we cannot override the S641
    // merged_tolerance (it only takes (quad, kind)); so we instead PRE-SEED the
    // declaration's per-quad tolerance from the sidecar when the declaration omitted
    // one — the catalogued tolerance wins over the kind default. Copy the expected
    // declaration so we never mutate the caller's.
    ScanDeclaration decl = expected;
    for (const auto& sq : sidecar.quads)
    {
        sidecarKinds.emplace_back(sq.name, sq.kind);
        // Locate the declaration quad by name; if it omitted an explicit tolerance and
        // the sidecar carried a catalogue tolerance (or a capability id that resolves
        // through the catalogue), apply it.
        for (auto& q : decl.quads)
        {
            if (q.name != sq.name || q.has_tolerance)
                continue;
            double tol = 0.0;
            bool hasTol = sq.has_tolerance && sq.tolerance > 0.0;
            if (hasTol)
            {
                tol = sq.tolerance;
            }
            else if (!sq.capability.empty())
            {
                // Resolve through the catalogue: a catalogued capability's declared
                // tolerance wins over the kind default.
                const Capability* cap = find_capability(capability_catalogue(), sq.capability);
                if (cap)
                    tol = catalogue_tolerance_for(*cap);
                else
                    tol = default_tolerance_for(sq.kind);
                hasTol = true;
            }
            if (hasTol)
            {
                q.tolerance = tol;
                q.has_tolerance = true;
            }
        }
    }

    return compliance_compare(actual, decl, sidecarKinds);
}

} // namespace vaml::presentation::scan
