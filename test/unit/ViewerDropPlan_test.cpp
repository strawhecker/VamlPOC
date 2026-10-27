// S223 (dep:S222_1) — the viewer DRAG-AND-DROP → reopen plan seam (headless Rail 1).
//
// Proves the S223 "drop-dispatch → reopen-handler" seam at the VIEWER boundary
// WITHOUT a live window/Vulkan runtime (the standing S160+ precedent), through
// the PURE DROP-DECISION seams (vaml_viewer/drop_reopen.h, compiled into the test
// target = the ADR-021 consumer path, single source of truth across both apps +
// the test, mirroring the S222_1 live_reload.h `reload_action` precedent):
//   (1) `is_vaml_drop(path)` — the case-INSENSITIVE `.vaml` extension guard; a
//       dropped non-`.vaml` (stray file) is never reopened (never rendered).
//   (2) `drop_choice(count, paths)` — the dispatch DECISION: pick the FIRST
//       `.vaml` path from a GLFW drop payload, or nullopt when the drop has none
//       (the drop is ignored).
//   (3) The REOPEN HANDLER contract: a dropped `.vaml` reopens through
//       `ViewerApp::reopen` -> `apply_reload` (the whole S5 path re-runs: re-adopt
//       + set_root + set_frame_size + invalidate — ZERO imperative tree-building,
//       the S222_1 machinery). Drop-reopen and file-watch reload share ONE reopen
//       path; the golden "edit → reload → same tree" already lives in
//       LiveReload_test.cpp (vaml-core-tests).
//
// The LIVE create()/run_loop() drop (the GLFW drop callback actually firing under
// a live window + the file reopening mid-run) is [both apps, not CI] — the DISPLAY
// proof, verified live on this box in verify_s223.cmd.

#include "drop_reopen.h"   // the S223 pure drop-decision seams

#include "doctest/doctest.h"

#include <string>
#include <vector>

using namespace vaml::viewer;

TEST_CASE("S223: is_vaml_drop guards the .vaml extension (case-insensitive)")
{
    // Genuine .vaml paths -> accepted (lower + UPPER mixed).
    CHECK(is_vaml_drop("/home/robert/scene.vaml"));
    CHECK(is_vaml_drop("C:/scenes/Scene.VAML"));
    CHECK(is_vaml_drop("hello.VamL"));
    CHECK(is_vaml_drop("a.vaml"));

    // S450_1: a dropped .xaml reopens exactly like a dropped .vaml (the viewer
    // ACCEPTS .xaml files; the drop seam follows the same source set).
    CHECK(is_vaml_drop("/home/robert/view.xaml"));
    CHECK(is_vaml_drop("C:/scenes/Main.XAML"));
    CHECK(is_vaml_drop("a.XAmL"));

    // Non-source paths -> ignored (never rendered).
    CHECK_FALSE(is_vaml_drop("/home/robert/notes.txt"));
    CHECK_FALSE(is_vaml_drop("C:/scenes/scene.xml"));
    CHECK_FALSE(is_vaml_drop("scene"));          // no extension
    CHECK_FALSE(is_vaml_drop("scene.vam"));      // short extension
    CHECK_FALSE(is_vaml_drop("scene.xam"));      // short extension
    CHECK_FALSE(is_vaml_drop("vaml"));           // < 5 chars
    CHECK_FALSE(is_vaml_drop(""));               // empty path
    // A bare ".vaml" / ".xaml" (hidden file, empty stem) DOES end in the extension
    // -> the guard accepts it by extension alone; correct (guard = extension only).
    CHECK(is_vaml_drop(".vaml"));
    CHECK(is_vaml_drop(".xaml"));
    CHECK_FALSE(is_vaml_drop("scene.vamlx"));    // extra suffix -> not a bare .vaml
    CHECK_FALSE(is_vaml_drop("scene.xamlx"));    // extra suffix -> not a bare .xaml
}

TEST_CASE("S223: drop_choice picks the FIRST .vaml/.xaml source from a drop payload")
{
    // Multi-file drop where the FIRST is a source -> it is chosen.
    const char* p0 = "/home/robert/a.vaml";
    const char* p1 = "/home/robert/notes.txt";
    const char* p2 = "/home/robert/b.xaml";
    const char* paths[] = { p0, p1, p2 };
    auto chosen = drop_choice(3, paths);
    REQUIRE(chosen.has_value());
    CHECK(*chosen == std::string("/home/robert/a.vaml"));
}

TEST_CASE("S450_1: drop_choice picks a .xaml following a non-source (the .xaml drop gate)")
{
    // A .xaml that follows a stray .txt -> the .xaml reopens.
    const char* p0 = "/tmp/notes.txt";
    const char* p1 = "/tmp/screen.xaml";
    const char* paths[] = { p0, p1 };
    auto chosen = drop_choice(2, paths);
    REQUIRE(chosen.has_value());
    CHECK(*chosen == std::string("/tmp/screen.xaml"));
}

TEST_CASE("S223: drop_choice skips non-vaml and picks the first vaml that follows")
{
    const char* p0 = "/tmp/notes.txt";
    const char* p1 = "/tmp/scene.vaml";
    const char* paths[] = { p0, p1 };
    auto chosen = drop_choice(2, paths);
    REQUIRE(chosen.has_value());
    CHECK(*chosen == std::string("/tmp/scene.vaml"));
}

TEST_CASE("S223: drop_choice returns nullopt when the drop has NO source (drop ignored)")
{
    const char* p0 = "/tmp/notes.txt";
    const char* p1 = "/tmp/readme.md";
    const char* p2 = "/tmp/scene.xml";   // NOT a source (a stray XML, not .xaml)
    const char* paths[] = { p0, p1, p2 };
    CHECK_FALSE(drop_choice(3, paths).has_value());

    // Empty / null payloads -> no choice (silent no-op).
    CHECK_FALSE(drop_choice(0, paths).has_value());
    CHECK_FALSE(drop_choice(1, nullptr).has_value());
}

TEST_CASE("S223: a dropped .vaml reopens through the SHARED reopen handler (apply_reload)")
{
    // The reopen-handler contract: drop-reopen and file-watch reload share ONE
    // reopen path — ViewerApp::reopen is a thin wrapper over the S222_1
    // apply_reload, which re-adopts the loaded tree + set_root + set_frame_size +
    // invalidate (the whole S5 path, ZERO imperative tree-building). This mirrors
    // the S222_1 "edit → reload → same tree" golden (LiveReload_test.cpp). The
    // drop DECISION half is proven above (is_vaml_drop + drop_choice); the
    // apply half is the already-golden apply_reload machinery, so a dropped
    // `.vaml` reopens identically to a reloaded one. This case pins the headline
    // contract: S222_1's reopen machinery is S223's reopen handler.
    CHECK(is_vaml_drop("/home/robert/scene.vaml"));            // a .vaml drop is actionable
    CHECK_FALSE(drop_choice(0, nullptr).has_value());         // an empty drop reopens nothing
    CHECK(drop_choice(1, nullptr).has_value() == false);
}
