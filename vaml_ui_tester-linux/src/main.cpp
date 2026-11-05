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
#include <vaml/core/Element.h>         // register_builtin_elements
#include <vaml/core/ValueConverter.h>  // register_builtin_converters

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

namespace {

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

    std::printf("PASS: '%s' loaded (%s)\n", path.c_str(), "S640 scaffold");
    return 0;
}
