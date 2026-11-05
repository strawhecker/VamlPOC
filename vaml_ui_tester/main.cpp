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
#include <vaml/core/Element.h>         // register_builtin_elements
#include <vaml/core/ValueConverter.h>  // register_builtin_converters

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

namespace {

// The usage banner (S641 extends it with the scan/compare modes). PURE entry.
constexpr const char* kUsage =
    "vaml_ui_tester <file.xaml>\n"
    "  Renders an authored .xaml through the real retained render path and (S641)\n"
    "  scans + compares it to the declared outcome. Exit: 0=PASS 1=FAIL 2=SKIP.\n"
    "  S640 scaffold: loads the authored doc, proving the BARE `Source` DP resolves.\n";

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fputs(kUsage, stderr);
        return 2;   // SKIP — no argument
    }
    const std::string path = argv[1];

    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // Load the authored doc through the REAL file->tree seam. For a `.xaml`
    // with an authored <Image Source="..."/>, the BARE `Source` DP (S640)
    // resolves — pre-S640 this THREW `unknown property 'Source'`. The S641
    // scanner extends this into the pull/render/scan/compare loop.
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
    // S641 scanner (next step) renders it + scrapes the rect + compares.
    std::printf("PASS: '%s' loaded (%s)\n", path.c_str(), "S640 scaffold");
    return 0;
}
