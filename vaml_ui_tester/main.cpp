// vaml_ui_tester — the DEDICATED Sprint 11r UI-compliance tester (Windows).
//
// S640 scaffold: the project SKELETON (scaffold only — its POWER lands at
// S641, the scanner). The dedicated executable whose ONLY job (per the Sprint
// 11r roadmap) is: take an authored `.xaml` argument, render it through the
// REAL retained render path (offscreen/windowed), SCAN the rendered UI for
// compliance, COMPARE to the expected outcome declared in that `.xaml`, and
// exit 0=PASS / 1=FAIL / 2=SKIP — the standalone process boundary a script
// drives, exactly like the S177/S446 real-WSI gates. It consumes ONLY the
// public surface (ADR-021).
//
// This S640 FILE is the scaffold: it takes the `.xaml` argv, registers the
// built-in element + converter tables, LOADS the authored doc through the
// REAL load path (proving the BARE `Source` DP resolution — the S640
// headline), and exits 0/1/2. The pull/render/scan/COMPARE loop itself is the
// S641 scanner's power (this file is scaffold-only per the roadmap).
//
// exit codes (the S641 script drives the per-file loop):
//   0 = PASS (loadable; the S641 scanner will confirm content on-screen)
//   1 = FAIL (the doc did not load / an authored element threw)
//   2 = SKIP (no argument)
// Every non-zero exit carries a stderr diagnostic — never silent.
//
// CONSUMER-APP PATTERN (ADR-021): includes ONLY public library headers
// (`<vaml/...>` from the public include roots) + the shared viewer seams it
// consumes; never an internal header from under src/<lib>/src.

#include "load_file.h"   // S221: the real file->tree seam (load_file / LoadResult)
// S641 — the DEDICATED tester NOW hosts the scanner's compare seam: parse the
// in-XAML SCAN DECLARATION (the primary oracle) + read the emitter sidecar
// (the forward-compatible secondary metadata) + the on-disk .bgra frames, then
// run vaml::presentation::scan::compliance_compare -> ScanReport + colored
// diff.png, exiting 0=PASS / 1=FAIL / 2=SKIP. Desktop-only, script-driven,
// NOT a CI lane (the roadmap's 11r directive); the compare seam itself is the
// headless, both-lane ctest-proven decision layer. ADR-021 consumer pattern.
#include <vaml/presentation/scan/ScanDeclaration.h>
#include <vaml/presentation/scan/Scanner.h>
#include <vaml/presentation/scan/EmitterMetadata.h>
#include <vaml/presentation/scan/PixelDiff.h>
#include <vaml/core/Element.h>         // register_builtin_elements
#include <vaml/core/ValueConverter.h>  // register_builtin_converters

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

// The usage banner (S641 extends the S640 scaffold with the compare mode).
constexpr const char* kUsage =
    "vaml_ui_tester <file.xaml>\n"
    "  Renders an authored .xaml through the real retained render path and (S641)\n"
    "  scans + compares it to the declared outcome. Exit: 0=PASS 1=FAIL 2=SKIP.\n"
    "\n"
    "  --compare <file.xaml> <actual.bgra> <expected.bgra> <sidecar.json>\n"
    "      [--report <out.txt>] [--diff <out.png>] [--actual-json <actual.json>]\n"
    "  Runs the S641 compliance compare over the given frames (the on-disk .bgra\n"
    "  ImageBuffers produced by the S640a RenderTarget GPU path or the WPF emitter)\n"
    "  against the authored .xaml's SCAN DECLARATION + the emitter sidecar. Writes a\n"
    "  ScanReport (per-datum PASS/FAIL detail) + a colored diff.png. Exit 0=PASS\n"
    "  1=FAIL 2=SKIP. Desktop-only, script-driven, NOT a CI lane.\n";

// Reads a tight w*h*4 BGRA frame; returns false (never silent) on error.
bool read_bgra(const std::string& path, uint32_t& w, uint32_t& h,
               std::vector<unsigned char>& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        std::fprintf(stderr, "FAIL: could not open .bgra '%s'\n", path.c_str());
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string& bytes = ss.str();
    if (bytes.empty())
    {
        std::fprintf(stderr, "FAIL: .bgra '%s' is empty\n", path.c_str());
        return false;
    }
    out.assign(bytes.begin(), bytes.end());
    // Derive dims from the byte count (a square-ish default; the sidecar/declaration
    // overrides). w*h*4 == bytes.
    const std::size_t n = out.size() / 4u;
    w = static_cast<uint32_t>(std::sqrt(static_cast<double>(n)));
    h = w ? static_cast<uint32_t>(n / w) : 0u;
    while (w && h && static_cast<std::size_t>(w) * h * 4u != out.size())
    {
        ++w;
        h = w ? static_cast<uint32_t>(n / w) : 0u;
    }
    if (w == 0u || h == 0u || static_cast<std::size_t>(w) * h * 4u != out.size())
    {
        std::fprintf(stderr, "FAIL: .bgra '%s' has no integer w*h*4 shape (%zu bytes)\n",
                     path.c_str(), out.size());
        return false;
    }
    return true;
}

// Builds a ScrapeGrid from a .bgra frame + the sidecar quads (the emitter's
// per-quad kind metadata seeds the element samples so the compare can resolve
// presence + rect — the forward-compatible metadata read). The pixel buffer is
// the authoritative scrape source.
bool build_scrape(const std::vector<unsigned char>& bgra, uint32_t w, uint32_t h,
                  const vaml::presentation::scan::EmitterMetadata& meta,
                  vaml::presentation::scan::ScrapeGrid& out)
{
    out.width = w;
    out.height = h;
    out.bgra.assign(bgra.begin(), bgra.end());
    for (const auto& q : meta.quads)
    {
        vaml::presentation::scan::ScrapeGrid::ElementSample s;
        s.name = q.name;
        s.x = static_cast<float>(q.x);
        s.y = static_cast<float>(q.y);
        s.w = static_cast<float>(q.w);
        s.h = static_cast<float>(q.h);
        out.elements.push_back(std::move(s));
    }
    return true;
}

std::string kind_to_string(vaml::presentation::scan::QuadKind k)
{
    using vaml::presentation::scan::QuadKind;
    switch (k)
    {
        case QuadKind::Solid: return "solid";
        case QuadKind::Text:  return "text";
        case QuadKind::Icon:  return "icon";
    }
    return "?";
}

std::string status_to_string(vaml::presentation::scan::DatumStatus s)
{
    using vaml::presentation::scan::DatumStatus;
    switch (s)
    {
        case DatumStatus::Pass:    return "PASS";
        case DatumStatus::Fail:    return "FAIL";
        case DatumStatus::Missing: return "MISSING";
    }
    return "?";
}

// Writes the ScanReport as a human/LLM-readable per-datum text file (or stdout).
void write_report(const vaml::presentation::scan::ScanReport& rep, const std::string& path)
{
    std::FILE* f = stdout;
    if (!path.empty())
        f = std::fopen(path.c_str(), "w");
    if (!f)
        f = stdout;

    std::fprintf(f, "SCAN REPORT (%u x %u) — %s\n", rep.width, rep.height,
                 rep.pass() ? "PASS" : "FAIL");
    std::fprintf(f, "  data=%d pass=%d fail=%d missing=%d\n",
                 rep.total(), rep.pass_count, rep.fail_count, rep.missing_count);
    for (const auto& d : rep.data)
    {
        std::fprintf(f, "  [%s] quad='%s' what='%s' actual=%.4f expected=%.4f tol=%.4f %s\n",
                     status_to_string(d.status).c_str(), d.quad.c_str(), d.what.c_str(),
                     d.actual, d.expected, d.tolerance, d.detail.c_str());
    }
    if (path.empty() || f == stdout)
        std::fflush(f);
    else
        std::fclose(f);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fputs(kUsage, stderr);
        return 2;   // SKIP — no argument
    }

    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // ---- S641: the script-driven COMPARE mode (desktop-only, NOT CI) ----
    if (std::string(argv[1]) == "--compare" && argc >= 6)
    {
        const std::string xamlPath = argv[2];
        const std::string actualPath = argv[3];
        const std::string expectPath = argv[4];
        const std::string sidecarPath = argv[5];
        std::string reportPath, diffPath, actualJsonPath;

        for (int i = 6; i < argc - 1; ++i)
        {
            const std::string a = argv[i];
            if (a == "--report")       reportPath = argv[++i];
            else if (a == "--diff")    diffPath = argv[++i];
            else if (a == "--actual-json") actualJsonPath = argv[++i];
        }

        // 1. Read + parse the in-XAML SCAN DECLARATION (the PRIMARY oracle).
        std::ifstream xin(xamlPath, std::ios::binary);
        if (!xin)
        {
            std::fprintf(stderr, "FAIL: could not open '%s'\n", xamlPath.c_str());
            return 1;
        }
        std::ostringstream xss;
        xss << xin.rdbuf();
        const vaml::presentation::scan::DeclarationResult decl =
            vaml::presentation::scan::parse_scan_declaration(xss.str());
        if (!decl.ok)
        {
            std::fprintf(stderr, "FAIL: SCAN DECLARATION parse error (%s): %s\n",
                         decl.ok ? "?" : "typed", decl.detail.c_str());
            return 1;
        }

        // 2. Read the emitter sidecar (the forward-compatible secondary metadata).
        const vaml::presentation::scan::MetadataResult meta =
            vaml::presentation::scan::read_emitter_metadata_file(
                actualJsonPath.empty() ? sidecarPath : actualJsonPath);
        if (!meta.ok)
        {
            std::fprintf(stderr, "FAIL: sidecar read error: %s\n", meta.detail.c_str());
            return 1;
        }

        // 3. Read the frames.
        uint32_t aw = 0, ah = 0, ew = 0, eh = 0;
        std::vector<unsigned char> abgra, ebgra;
        if (!read_bgra(actualPath, aw, ah, abgra)) return 1;
        if (!read_bgra(expectPath, ew, eh, ebgra)) return 1;
        // The declared frame is authoritative for dims when present; else sidecar.
        if (decl.declaration.width)  aw = decl.declaration.width;
        if (decl.declaration.height) ah = decl.declaration.height;

        // 4. Build the actual ScrapeGrid + run the compare against the declaration
        //    + the sidecar kinds (the secondary tolerance oracle).
        vaml::presentation::scan::ScrapeGrid grid;
        build_scrape(abgra, aw, ah, meta.metadata, grid);
        std::vector<std::pair<std::string, vaml::presentation::scan::QuadKind>> kinds;
        for (const auto& q : meta.metadata.quads)
            kinds.emplace_back(q.name, q.kind);
        const vaml::presentation::scan::ScanReport rep =
            vaml::presentation::scan::compliance_compare(grid, decl.declaration, kinds);

        // 5. Write the report + the colored diff.png (the GIMP-style tool).
        write_report(rep, reportPath);
        if (!diffPath.empty())
        {
            vaml::presentation::Impl::ImageBuffer expected;
            expected.width = decl.declaration.width ? decl.declaration.width : ew;
            expected.height = decl.declaration.height ? decl.declaration.height : eh;
            expected.bgra.assign(ebgra.begin(), ebgra.end());
            vaml::presentation::Impl::ImageBuffer actualBuf;
            actualBuf.width = aw; actualBuf.height = ah;
            actualBuf.bgra.assign(abgra.begin(), abgra.end());
            // The compare already exercises per-quad tolerance; the diff tool adds the
            // per-pixel color-coded image for a human/LLM. Use a loose global tolerance
            // for the visual diff (the structural report is authoritative).
            vaml::presentation::scan::PixelDiff diff =
                vaml::presentation::scan::pixel_diff(actualBuf, expected, 0.10);
            if (!vaml::presentation::scan::pixel_diff_write_png(diff, diffPath))
            {
                std::fprintf(stderr, "FAIL: could not write diff.png '%s'\n", diffPath.c_str());
                return 1;
            }
        }

        // 6. Exit 0=PASS 1=FAIL 2=SKIP.
        if (rep.pass())
        {
            std::printf("PASS: '%s' (data=%d pass=%d)\n", xamlPath.c_str(),
                        rep.total(), rep.pass_count);
            return 0;
        }
        std::printf("FAIL: '%s' (data=%d pass=%d fail=%d missing=%d)\n", xamlPath.c_str(),
                    rep.total(), rep.pass_count, rep.fail_count, rep.missing_count);
        return 1;
    }

    // ---- S640 scaffold fallback (load-only, proving the bare Source DP) ----
    const std::string path = argv[1];

    if (path == "--help" || path == "-h")
    {
        std::fputs(kUsage, stdout);
        return 0;
    }

    vaml::LoadResult r = vaml::load_file(path);
    if (!r.ok)
    {
        std::fprintf(stderr, "FAIL: could not load '%s' (%s)\n",
                     path.c_str(), r.detail.c_str());
        return 1;
    }
    if (r.root == nullptr)
    {
        std::fprintf(stderr, "FAIL: '%s' loaded but produced no root\n", path.c_str());
        return 1;
    }

    // S640 scaffold: the authored `.xaml` loaded through the REAL path. The
    // S641 scanner compares it when a `--compare` lane is driven.
    std::printf("PASS: '%s' loaded\n", path.c_str());
    return 0;
}
