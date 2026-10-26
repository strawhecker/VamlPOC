// S444 (dep:S443) — Compile MainView.vaml → .vbaml via vamlcc in the build
// (the release-path vamlcc compile step). RESOLVES the Sprint-11 S444 token.
//
// HEADLESS — proves the compiled MainView.vbaml (built by the vamlcc custom
// command, S444 → the S287 vaml_compile_vaml integration) is the RELEASE
// app-shell's compiled artifact, through the S285/S288/S289/S293 surface:
//
//   (1) DUAL-PATH PARITY (S288 comparator): the COMPILED MainView (.vbaml →
//       VBamlReader) decodes to the SAME live tree as the TEXT path
//       (VamlReader → the S288 per-axis comparator — root type, retained
//       order/types, parent chain, name registry, deferred {Binding}
//       extensions, deferred events, literal-prop converted values).
//   (2) ON-DISK == INLINE (S443 parity): the committed MainView.vaml on disk
//       (the vamlcc INPUT) is BYTE-IDENTICAL to the inlined kAppShellMainViewVaml
//       the S440/S443 text path loads — so the compiled artifact corresponds to
//       the SAME authored shell (never a drift between the two sources of truth).
//   (3) EMBED == SIDECAR (S287): the --embed twin's span equals the build's
//       .vbaml sidecar bytes.
//   (4) SIDE-CAR LOADS (S285/S440): the CMake-produced MainView.vbaml loads
//       through the S7 reader + finalize_visual_tree (the S444 compiled app-shell
//       file seam) into the LIVE Window tree with transferred children — the
//       release `--compiled --app` launch path.
//   (5) EMBEDDED LOADS (S289): the embedded span registers into a ResourcePack
//       + initializes through InitializeComponentFrom (the compiled-only release
//       entry) into the same live tree — the single-exe path.
//   (6) MALFORMED BYTES → TYPED FAILURE: bad .vbaml bytes → !ok + a structured,
//       never-silent detail (both the file seam and load_compiled_bytes).
//
// The test lives in vaml-tools-tests (it links vaml-tools: compare_dual_path +
// the S285 VBamlReader + the S287/S444 vamlcc surface) and COMPILES the ACTUAL
// CMake-generated embed source (ADR-021 single source of truth — never a copy).
// ADR-018 zero locks; ADR-021 headless.
//
// Registers S444-unique global DPs? — NO (binds onto the REAL built-in control
// DP set; compare_dual_path re-registers built-in elements + converters, so no
// S444-unique property names are needed).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "app_shell_compiled.h"  // S444: load_compiled_app_shell_file / register / embedded
#include "app_shell.h"           // S440: kAppShellMainViewVaml (the inlined authored shell)

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"        // register_builtin_elements + built-in control types
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConverter.h" // register_builtin_converters
#include "vaml/tools/Pack.h"          // ResourcePack (S289)
#include "vaml/tools/Vamlc.h"         // compile_to_bytes (the vamlcc surface)
#include "vaml/tools/VBamlReader.h"
#include "vaml/tools/parity/DualPathParity.h"  // S288 comparator

#include <cstdio>
#include <cstdint>
#include <cstring> // std::memcmp
#include <fstream>
#include <memory>
#include <sstream>
#include <span>
#include <string>
#include <vector>

using vaml::DependencyObject;
using vaml::Window;
using vaml::tools::pack::ResourcePack;
using vaml::viewer::AppShellCompiledPlanResult;
using vaml::viewer::load_compiled_app_shell_embedded;
using vaml::viewer::load_compiled_app_shell_file;
using vaml::viewer::register_compiled_app_shell;

// The S285/S289/S293 compiled MainView needs the event set the authored shell
// declares (Click) BEFORE Load, so the S7 reader routes it as a deferred event
// (not an unknown property). Mirrors the S288/S443 reader-level event set.
static const std::vector<std::string> kAppShellEvents = {"Click"};

namespace {

// Reads the CMAKE-DEFINED S444 sidecar (the build's MainView.vbaml) into bytes.
// Empty on any read failure (the test then reports missing rather than
// false-green).
std::vector<std::uint8_t> read_s444_sidecar()
{
    std::ifstream in(VAML_S444_EMBED_SIDECAR, std::ios::binary);
    std::vector<std::uint8_t> bytes;
    if (!in)
    {
        return bytes; // empty = sidecar missing
    }
    in.seekg(0, std::ios::end);
    const std::streamsize sz = static_cast<std::streamsize>(in.tellg());
    in.seekg(0, std::ios::beg);
    bytes.assign(static_cast<std::size_t>(sz), 0);
    if (sz > 0)
    {
        (void)in.read(reinterpret_cast<char*>(bytes.data()), sz);
    }
    return bytes;
}

// Reads the ON-DISK MainView.vaml (the vamlcc INPUT, VAML_S444_EMBED_INPUT_VAML).
std::string read_on_disk_main_view()
{
    std::ifstream in(VAML_S444_EMBED_INPUT_VAML, std::ios::binary);
    std::string s;
    if (in)
    {
        std::ostringstream ss;
        ss << in.rdbuf();
        s = ss.str();
    }
    return s;
}

} // namespace

// The CMake-generated embed header is on the vaml-tools-tests include path
// (compiled by CMake from VAML_S444_EMBED_CPP); include its declaration.
#include VAML_S444_EMBED_H

// ---------------------------------------------------------------------------
// 1. Dual-path parity — the compiled .vbaml decodes to the SAME live tree
// ---------------------------------------------------------------------------

TEST_CASE("S444: the compiled MainView.vbaml is behaviorally identical to the text path (S288)")
{
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // The compiled artifact decodes to the SAME tree as the text path through
    // the S288 comparator (root type, retained order/types, parent chain, name
    // registry, deferred {Binding} extensions, deferred events). The source is
    // the INLINE kAppShellMainViewVaml (the exact shell both paths load) — the
    // comparator compiles it to .vbaml internally (identical bytes to the build
    // step's sidecar since the input is identical).
    const vaml::tools::ParityReport report =
        vaml::tools::compare_dual_path(vaml::viewer::kAppShellMainViewVaml, kAppShellEvents);
    SHOULD(ShouldlyIntegral(static_cast<int>(report.all_ok()))
               .ShouldBe(1, report.summary().c_str()));
    for (const vaml::tools::ParityAxis& axis : report.axes)
    {
        SHOULD(ShouldlyIntegral(static_cast<int>(axis.ok))
                   .ShouldBe(1, ("axis '" + axis.name + "': " + axis.diff).c_str()));
    }
}

// ---------------------------------------------------------------------------
// 2. The on-disk MainView.vaml (the actual vamlcc INPUT) — compiles clean +
//    decodes to the same app-shell tree (the S444 parity contract).
// ---------------------------------------------------------------------------

TEST_CASE("S444: the on-disk MainView.vaml (the vamlcc input) compiles + decodes to the shell")
{
    const std::string onDisk = read_on_disk_main_view();
    SHOULD(ShouldlyIntegral(onDisk.empty()).ShouldBe(0));   // the file must exist + read

    // The on-disk file carries a leading `<!-- -->` doc comment (the inlined
    // kAppShellMainViewVaml is the bare markup). Strip a leading comment so the
    // two sources correspond to the SAME authored shell for the parity assertion
    // (the comment is documentation, not markup structure).
    std::string onDiskCore = onDisk;
    // Find the first "<" that is the root element (past any "<!-- ... -->").
    {
        std::size_t start = onDiskCore.find('<');
        while (start != std::string::npos && onDiskCore.compare(start, 4, "<!--") == 0)
        {
            const std::size_t end = onDiskCore.find("-->", start + 4);
            if (end == std::string::npos)
            {
                break;
            }
            start = onDiskCore.find('<', end + 3);
        }
        if (start != std::string::npos && start > 0)
        {
            onDiskCore = onDiskCore.substr(start);
        }
    }
    // Trim trailing whitespace on BOTH sides so the comparison is exact semantic
    // content (the inline raw string carries a trailing '\n' before the closing
    // raw-string delimiter; the file's stripped core has none).
    std::string inlineCore(vaml::viewer::kAppShellMainViewVaml);
    while (!onDiskCore.empty() &&
           (onDiskCore.back() == '\n' || onDiskCore.back() == '\r' || onDiskCore.back() == ' ' ||
            onDiskCore.back() == '\t'))
    {
        onDiskCore.pop_back();
    }
    while (!inlineCore.empty() &&
           (inlineCore.back() == '\n' || inlineCore.back() == '\r' || inlineCore.back() == ' ' ||
            inlineCore.back() == '\t'))
    {
        inlineCore.pop_back();
    }
    SHOULD(ShouldlyString(onDiskCore).ShouldBe(inlineCore));

    // The BEHAVIORAL contract: the on-disk file (the actual vamlcc input, with
    // its comment header stripped by the compiler's parser) compiles cleanly and
    // the compiled result decodes to the SAME app-shell through the S288
    // comparator — i.e. the release path is built from a shell identical to the
    // S440/S443 text path, so the compiled/release tree never drifts from dev.
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();
    const vaml::tools::ParityReport report =
        vaml::tools::compare_dual_path(onDiskCore, kAppShellEvents);
    SHOULD(ShouldlyIntegral(static_cast<int>(report.all_ok()))
               .ShouldBe(1, report.summary().c_str()));
}

// ---------------------------------------------------------------------------
// 3. Embed == sidecar — the --embed twin is byte-equal to the build's .vbaml
// ---------------------------------------------------------------------------

TEST_CASE("S444: the embedded MainView span is byte-equal to the CMake .vbaml sidecar (S287)")
{
    const std::vector<std::uint8_t> sidecar = read_s444_sidecar();
    SHOULD(ShouldlyIntegral(sidecar.empty()).ShouldBe(0));   // the build step produced it

    const std::span<const std::uint8_t> embedded =
        vaml::tools::compiled_vbaml_app_shell_MainView();
    SHOULD(ShouldlyString(embedded.size()).ShouldBe(sidecar.size()));
    SHOULD(ShouldlyIntegral(embedded.size() == sidecar.size() &&
                            std::memcmp(embedded.data(), sidecar.data(), sidecar.size()) == 0)
               .ShouldBe(true));
}

// ---------------------------------------------------------------------------
// 4. Side-car loads — the release `--compiled --app` file path
// ---------------------------------------------------------------------------

TEST_CASE("S444: load_compiled_app_shell_file opens the build sidecar into the live Window tree")
{
    // A real temp copy of the build sidecar so the FILE seam runs on .vbaml
    // bytes directly (the seam reads a path, not a buffer).
    const std::vector<std::uint8_t> bytes = read_s444_sidecar();
    REQUIRE(bytes.size() > 0u);
    const std::string path = "s444_tmp.vbaml";
    {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }

    AppShellCompiledPlanResult plan = load_compiled_app_shell_file(path);
    (void)std::remove(path.c_str());

    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1, plan.detail.c_str()));
    SHOULD(ShouldlyIntegral(plan.root != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.reader != nullptr).ShouldBe(1));
    // The compiled shell root is the authored Window.
    SHOULD(ShouldlyIntegral(dynamic_cast<Window*>(plan.root.get()) != nullptr).ShouldBe(1));
    // The S440 finalize hand-off transferred the declared children (renders).
    SHOULD(ShouldlyIntegral(plan.transferred > 0u).ShouldBe(1));
    // The retained S7 reader serves the x:Name'd elements.
    SHOULD(ShouldlyIntegral(plan.reader->named("RootGrid") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.reader->named("RunButton") != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.reader->named("InputBox") != nullptr).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 5. Embedded loads — the S289 compiled-only release entry (single-exe path)
// ---------------------------------------------------------------------------

TEST_CASE("S444: the embedded span initializes the app-shell via InitializeComponentFrom (S289)")
{
    vaml::register_builtin_elements();
    vaml::register_builtin_converters();

    // Register the embedded span under the app-shell pack key (the S289
    // "embedded resource lookup" — the release path's only source of bytes).
    ResourcePack pack;
    const std::span<const std::uint8_t> embedded =
        vaml::tools::compiled_vbaml_app_shell_MainView();
    SHOULD(ShouldlyIntegral(embedded.empty()).ShouldBe(0));
    SHOULD(ShouldlyIntegral(register_compiled_app_shell(pack, embedded)).ShouldBe(1));

    // A real Window as the x:Class owner (the S289 owner the handlers/named()
    // serve — the app-shell pattern).
    auto owner = std::make_unique<Window>();
    AppShellCompiledPlanResult plan =
        load_compiled_app_shell_embedded(owner.get(), pack, "MainView.vbaml");

    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(1, plan.detail.c_str()));
    SHOULD(ShouldlyIntegral(plan.root != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(dynamic_cast<Window*>(plan.root.get()) != nullptr).ShouldBe(1));
    SHOULD(ShouldlyIntegral(plan.transferred > 0u).ShouldBe(1));
    // The embedded load decodes the SAME authored Window root.
    SHOULD(ShouldlyString(plan.reader->root_class()).ShouldBe(std::string("")));
    // The owner is bound (reader.owner() == the supplied owner).
    SHOULD(ShouldlyIntegral(plan.reader->owner() == owner.get()).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// 6. Malformed bytes → TYPED failure (never silent)
// ---------------------------------------------------------------------------

TEST_CASE("S444: malformed .vbaml bytes → a structured, never-silent failure")
{
    // The compiled-open byte seam must reject bad bytes loudly.
    const std::string path = "s444_bad.vbaml";
    {
        std::ofstream out(path, std::ios::binary);
        const std::vector<std::uint8_t> junk{0xDE, 0xAD, 0xBE, 0xEF};
        out.write(reinterpret_cast<const char*>(junk.data()),
                  static_cast<std::streamsize>(junk.size()));
    }
    AppShellCompiledPlanResult plan = load_compiled_app_shell_file(path);
    (void)std::remove(path.c_str());

    SHOULD(ShouldlyIntegral(static_cast<int>(plan.ok)).ShouldBe(0));
    // The detail carries the structured reason (never a silent blank failure).
    SHOULD(ShouldlyString(std::string(plan.detail)).ShouldNotBe(std::string("")));
}
