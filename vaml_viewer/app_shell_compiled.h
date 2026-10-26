// vaml_viewer — the S444 COMPILED APP-SHELL seam (dep:S443; the "vamlcc
// compile step" token).
//
// THE RELEASE-PATH APP-SHELL LOAD. S440/S443 load the authored MainView.vaml as
// TEXT through the S2 VamlReader (the DEV tool). S444 is the RELEASE path: the
// on-disk MainView.vaml is COMPILED at build time by vamlcc into a .vbaml
// SIDE-CAR (+ an EMBEDDED C++ twin), and the app-shell can be launched from the
// COMPILED bytes exactly as the S293 compiled-open viewer renders a .vbaml.
//
// This header is the COMMON seam (per the S444 "common folder" directive —
// ADR-021, the standing S221/S222_1/S293/app_shell_binding precedent): compiled
// by BOTH apps + the test target from the vaml_viewer include path, so there is
// ONE source of truth for "load the compiled app-shell" on Windows + Linux + in
// the S444 ctest.
//
// THREE real, headless-testable entries (all ADR-021 public-surface-only):
//   * `load_compiled_app_shell_file(vbamlPath)` — the SIDE-CAR path: opens a
//     compiled `.vbaml` via the S293 `load_compiled_file` (S285 VBamlReader),
//     then finalize_visual_tree (the S440 render hand-off) so the compiled tree
//     renders its declared children. This is what `--compiled --app` uses with
//     the build-produced sidecar.
//   * `register_compiled_app_shell(pack, symbol)` — the EMBEDDED path: the S289
//     ResourcePack + the S287 `compiled_vbaml_<symbol>()` span -> the pack's
//     "embedded resource lookup". The release single-exe path registers the
//     embedded MainView bytes under the app-shell pack key.
//   * `load_compiled_app_shell_embedded(owner, pack, packUri)` — the
//     EMBEDDED-load path: S289 `InitializeComponentFrom` (the compiled-only
//     release entry — x:Class owner + ResourcePack, NEVER text) + finalize. This
//     is the S444 release proof that the app-shell can be constructed from the
//     embedded bytes alone (no file, no .vaml, no text reader).
//
//   * `AppShellCompiledPlanResult` — the shared outcome carrying the live tree +
//     the RETAINED S7 reader + the finalized child count + a structured detail.
//     The retained reader keeps the S286 named() registry + the S285 non-root
//     retained objects alive for the app's lifetime (the S293 compiled-open
//     pattern).
//
// SCOPE (recorded NO-GAP, not silent): the S443 {Binding}/{Command} wiring on
// the COMPILED path is deferred to S445 (the compiled reader defers {Binding}
// per S286; the release path hands that to the S445 E2E slice, exactly as S443
// recorded for the text path's live wires). This seam proves the compiled tree
// is BEHAVIORALLY identical to the text path (the S288 comparator — the S444
// test asserts it) + renders it; the interactive wiring is the S445 step.
//
// NO MINIMAL REAL introduced — every entry is REAL permanent code (kept-and-
// extended by S445). ADR-018: ZERO new locks (single-threaded on the viewer's
// UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"   // the owned root
#include "vaml/core/UIElement.h"          // UIElement (the renderable + child host)
#include "vaml/tools/Pack.h"              // pack::ResourcePack + resolve_pack_uri (S289)
#include "vaml/tools/VBamlReader.h"       // vaml::tools::VBamlReader (S7 reader)
#include "compiled_open.h"                // S293: load_compiled_file + CompiledOpenResult

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace vaml::viewer {

// The outcome of a compiled app-shell plan run.
//
// `reader` is the RETAINED S7 VBamlReader (owned on the heap) so the S286
// named() registry + the S285 non-root retained objects stay alive with the
// tree for the app's lifetime (the S293 compiled-open pattern — the tree borrows
// its non-root objects via parent links, so the app must hold both `root` AND
// `reader`). `ok` is true only when the compiled bytes decoded cleanly + the
// S440 finalize hand-off transferred children (the tree RENDERS).
struct AppShellCompiledPlanResult
{
    bool ok = false;                                  // decoded + finalized cleanly
    std::unique_ptr<vaml::UIElement> root;            // the live shell tree (UIElement root)
    std::unique_ptr<vaml::tools::VBamlReader> reader; // RETAINED S7 reader (keeps named()/retained alive)
    std::size_t transferred = 0;                      // the S440 finalized child count (renders)
    std::string detail;                               // structured failure reason (never silent)
};

// The sidecar path the `--compiled --app` launch opens (the build-produced
// MainView.vbaml from the vamlcc compile step). Each build system DEFINED the
// macro VAML_APPSHELL_SIDECAR to its generated sidecar path (CMake passes the
// compiler's binary dir; the vcxproj passes its output dir); this seam returns
// that path, or a sane default ("MainView.vbaml" in the working dir) when the
// macro is absent (a plain build without the compile step). Pure + headless.
[[nodiscard]] inline std::string compiled_app_shell_sidecar_path()
{
#ifdef VAML_APPSHELL_SIDECAR
    return std::string(VAML_APPSHELL_SIDECAR);
#else
    return std::string("MainView.vbaml");
#endif
}

namespace detail {

// The app-shell's compiled reader-level EVENT SET — the authored MainView.vaml
// declares Click="OnClick" (S443). Registering "Click" on the S7 reader BEFORE
// Load routes the event-shaped key to a deferred event handler (never an
// unknown-property throw) — the exact S288/S443 reader-level event contract,
// shared by the sidecar-file path + the embedded path + the S444 test.
inline const std::vector<std::string>& app_shell_events()
{
    static const std::vector<std::string> events = {"Click"};
    return events;
}

// A SMALL PACK-IDENTITY CONSTANT — the app-shell's compiled resource key. This
// is the S289 pack key under which the compiled/embedded MainView bytes are
// registered + the pack:// identity InitializeComponentFrom resolves. Reused by
// the file path (the sidecar) and the embedded path (the S287 span). Common for
// BOTH apps + the test (single source of truth).
inline constexpr const char* kAppShellPackUri = "MainView.vbaml";

// The S444 HELPER: after a compiled load, run the S440 finalize_visual_tree
// hand-off + produce the shared result. Shared by the file + embedded entries so
// neither drifts (NO-GAP). `reader`/`root` are moved in; `reader` decoded `root`.
[[nodiscard]] inline AppShellCompiledPlanResult
finalize_compiled_app_shell(std::unique_ptr<vaml::tools::VBamlReader> reader,
                            std::unique_ptr<vaml::DependencyObject> loaded)
{
    AppShellCompiledPlanResult r;
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(loaded.get());
    if (uiRoot == nullptr)
    {
        r.ok = false;
        r.detail = "app-shell: compiled MainView root is not a UIElement (cannot render)";
        return r;
    }
    // The S440 hand-off: render the DECLARED children, not just the root.
    r.transferred = reader->finalize_visual_tree();
    r.ok = (r.transferred > 0);
    if (!r.ok)
    {
        r.detail = "app-shell: compiled finalize_visual_tree() transferred 0 children "
                   "(declared UI would not render)";
        return r;
    }
    r.root = std::unique_ptr<vaml::UIElement>(static_cast<vaml::UIElement*>(loaded.release()));
    r.reader = std::move(reader);
    return r;
}

} // namespace detail

// THE SIDE-CAR COMPILED APP-SHELL LOAD (the `--compiled --app` file path). Opens
// a compiled `.vbaml` at `vbamlPath` through the S293 `load_compiled_file` seam
// (S285 VBamlReader), then runs the S440 finalize hand-off. Typical usage passes
// the build-produced MainView.vbaml sidecar path. A malformed/missing file or a
// 0-child finalize is a structured, never-silent failure.
[[nodiscard]] inline AppShellCompiledPlanResult
load_compiled_app_shell_file(const std::string& vbamlPath)
{
    vaml::viewer::CompiledOpenResult c =
        vaml::viewer::load_compiled_file(vbamlPath, &detail::app_shell_events());
    if (!c.ok || !c.root)
    {
        AppShellCompiledPlanResult r;
        r.ok = false;
        r.detail = c.ok ? ("app-shell: compiled open produced no root; " + c.detail)
                        : ("app-shell: compiled open failed: " + c.detail);
        return r;
    }
    // The reader owns the tree's non-root retained objects (the S293 pattern);
    // hand it + the root to the shared finalize helper.
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(c.root.get());
    if (uiRoot == nullptr)
    {
        AppShellCompiledPlanResult r;
        r.ok = false;
        r.detail = "app-shell: compiled open root is not a UIElement (cannot render)";
        return r;
    }
    return detail::finalize_compiled_app_shell(std::move(c.reader), std::move(c.root));
}

// THE EMBEDDED REGISTRATION (the S289 "embedded resource lookup"). Registers the
// S287 `compiled_vbaml_<symbol>()` span under the app-shell pack key so
// InitializeComponentFrom can resolve it. Returns false (never silent) when the
// span is empty (a build that produced no embedded bytes).
[[nodiscard]] inline bool
register_compiled_app_shell(vaml::tools::pack::ResourcePack& pack,
                            std::span<const std::uint8_t> embedded)
{
    if (embedded.empty())
    {
        return false;
    }
    const std::vector<std::uint8_t> bytes(embedded.begin(), embedded.end());
    pack.add_embedded(detail::kAppShellPackUri, std::span<const std::uint8_t>(bytes));
    return pack.has(detail::kAppShellPackUri);
}

// THE EMBEDDED-LOAD PATH (the S289 release entry). Resolves the compiled
// MainView bytes through `pack` via the S289 `InitializeComponentFrom` (the
// COMPILED-ONLY entry — x:Class owner + ResourcePack identity, never a text/
// .vaml parameter), binds `owner` (the app-shell Window root owner), runs the
// S440 finalize hand-off, and returns the shared result. A malformed/unknown
// pack identity or a non-UIElement root is a structured, never-silent failure.
//
// `owner` is the S289 x:Class owner (the app-shell Window the handlers/named()
// serve) — non-owning; the caller (the app or the test) keeps it alive. The
// returned reader retains the tree's non-root objects.
[[nodiscard]] inline AppShellCompiledPlanResult
load_compiled_app_shell_embedded(vaml::DependencyObject* owner,
                                 vaml::tools::pack::ResourcePack& pack,
                                 std::string_view packUri = detail::kAppShellPackUri)
{
    AppShellCompiledPlanResult r;
    try
    {
        if (owner == nullptr)
        {
            r.ok = false;
            r.detail = "app-shell: compiled embedded load requires a non-null owner";
            return r;
        }
        auto reader = std::make_unique<vaml::tools::VBamlReader>();
        // Register the app-shell's reader-level event set (Click) so the
        // event-shaped key routes to a deferred handler, never an
        // unknown-property throw (the S288/S443 contract).
        for (const std::string& ev : detail::app_shell_events())
        {
            reader->register_event(ev);
        }
        // The compiled-only release entry: the x:Class owner + the pack identity.
        std::unique_ptr<vaml::DependencyObject> loaded =
            reader->InitializeComponentFrom(owner, pack, packUri);
        if (!loaded)
        {
            r.ok = false;
            r.detail = "app-shell: InitializeComponentFrom produced no root";
            return r;
        }
        return detail::finalize_compiled_app_shell(std::move(reader), std::move(loaded));
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();   // never silent
    }
    return r;
}

} // namespace vaml::viewer
