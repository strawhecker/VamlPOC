// vaml_ui_tester — the DEDICATED Sprint 11r UI-compliance tester (Linux).
//
// S640 scaffold (mirror of vaml_ui_tester/main.cpp — the Windows twin, same
// exit-code contract). Takes an authored `.xaml` argument, registers the
// built-in element + converter tables, LOADS the doc through the REAL
// file->tree seam (proving the BARE `Source` DP resolution — the S640
// headline), and exits 0=PASS / 1=FAIL / 2=SKIP. The pull/render/scan/COMPARE
// loop is the S641 scanner's power (recorded NO-GAP, not silent).
//
// exit codes (the S641 script drives the per-file loop):
//   0 = PASS (loadable; the S641 scanner will confirm content on-screen)
//   1 = FAIL (the doc did not load / an authored element threw)
//   2 = SKIP (no argument)
// Every non-zero exit carries a stderr diagnostic — never silent.
//
// CONSUMER-APP PATTERN (ADR-021): includes ONLY public library headers.

#include "load_file.h"   // S221: the real file->tree seam (load_file / LoadResult)
// S641 — the headless twin of vaml_ui_tester/main.cpp hosts the SAME scanner
// compare seam (--compare over on-disk .bgra + sidecar + the authored .xaml's
// SCAN DECLARATION). The compare DECISION layer is ctest-proven on both CI
// lanes (headless, no Vulkan). This file builds the headless runner for parity;
// the REAL GPU render lane (S640a RenderTarget) is the [both apps, not CI]
// Windows host driver.
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

constexpr const char* kUsage =
    "vaml_ui_tester <file.xaml>\n"
    "  Renders an authored .xaml through the real retained render path and (S641)\n"
    "  scans + compares it to the declared outcome. Exit: 0=PASS 1=FAIL 2=SKIP.\n"
    "\n"
    "  --compare <file.xaml> <actual.bgra> <expected.bgra> <sidecar.json>\n"
    "      [--report <out.txt>] [--diff <out.png>]\n"
    "  Runs the S641 compliance compare over on-disk .bgra frames + the authored\n"
    "  SCAN DECLARATION + the emitter sidecar (headless; NOT a CI lane for the GPU\n"
    "  render — that is the Windows host's [both apps, not CI] lane).\n";

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
        std::fflush(stdout);
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

    if (std::string(argv[1]) == "--compare" && argc >= 6)
    {
        const std::string xamlPath = argv[2];
        const std::string actualPath = argv[3];
        const std::string expectPath = argv[4];
        const std::string sidecarPath = argv[5];
        std::string reportPath, diffPath;
        for (int i = 6; i < argc - 1; ++i)
        {
            const std::string a = argv[i];
            if (a == "--report") reportPath = argv[++i];
            else if (a == "--diff") diffPath = argv[++i];
        }

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
            std::fprintf(stderr, "FAIL: SCAN DECLARATION parse error: %s\n", decl.detail.c_str());
            return 1;
        }

        const vaml::presentation::scan::MetadataResult meta =
            vaml::presentation::scan::read_emitter_metadata_file(sidecarPath);
        if (!meta.ok)
        {
            std::fprintf(stderr, "FAIL: sidecar read error: %s\n", meta.detail.c_str());
            return 1;
        }

        uint32_t aw = 0, ah = 0, ew = 0, eh = 0;
        std::vector<unsigned char> abgra, ebgra;
        if (!read_bgra(actualPath, aw, ah, abgra)) return 1;
        if (!read_bgra(expectPath, ew, eh, ebgra)) return 1;
        if (decl.declaration.width)  aw = decl.declaration.width;
        if (decl.declaration.height) ah = decl.declaration.height;

        vaml::presentation::scan::ScrapeGrid grid;
        grid.width = aw; grid.height = ah;
        grid.bgra.assign(abgra.begin(), abgra.end());
        for (const auto& q : meta.metadata.quads)
        {
            vaml::presentation::scan::ScrapeGrid::ElementSample s;
            s.name = q.name;
            s.x = static_cast<float>(q.x);
            s.y = static_cast<float>(q.y);
            s.w = static_cast<float>(q.w);
            s.h = static_cast<float>(q.h);
            grid.elements.push_back(std::move(s));
        }
        std::vector<std::pair<std::string, vaml::presentation::scan::QuadKind>> kinds;
        for (const auto& q : meta.metadata.quads)
            kinds.emplace_back(q.name, q.kind);
        const vaml::presentation::scan::ScanReport rep =
            vaml::presentation::scan::compliance_compare(grid, decl.declaration, kinds);

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
            vaml::presentation::scan::PixelDiff diff =
                vaml::presentation::scan::pixel_diff(actualBuf, expected, 0.10);
            if (!vaml::presentation::scan::pixel_diff_write_png(diff, diffPath))
            {
                std::fprintf(stderr, "FAIL: could not write diff.png '%s'\n", diffPath.c_str());
                return 1;
            }
        }

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

    std::printf("PASS: '%s' loaded\n", path.c_str());
    return 0;
}
