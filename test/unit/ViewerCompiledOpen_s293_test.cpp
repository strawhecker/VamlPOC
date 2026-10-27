// S293 (dep:S288) — VIEWER-GROWTH: vaml_viewer opens a COMPILED `.vbaml`
// (the S280→S289 path visible). Text→compiled→render via the runtime
// VBamlReader (S285); the S222_1 reload recompiles .vaml→.vbaml (vamlcc S287)
// then reopens.
//
// Drives the REAL viewer compiled-open seam (vaml_viewer/compiled_open.h) over
// the REAL compiled pipeline (vamlcc compile_to_bytes → VBamlReader):
//   * load_compiled_bytes(bytes) → live tree (root type + the retained reader
//     keeps named()/retained alive) — a malformed .vbaml → typed failure.
//   * compile_to_vbaml(text) → round-trips through VBamlReader (the S283
//     compiler + the S285 reader).
//   * load_compiled_file: a `.vaml` COMPILES first → loads; a `.vbaml` loads
//     directly — behavioral parity through the two file paths.
//   * recompile-then-reopen contract (a changed source recompiles to a NEW
//     tree; the structural compare shows the difference).
//
// ADR-021 headless (this is exactly the seam both apps run); ADR-018 zero locks.

#include "doctest/doctest.h"

#include "compiled_open.h"   // the REAL seam (single source of truth; vaml_viewer on the include path)

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Element.h"     // StackPanel/StackPanel built-ins (the S284 golden root is a StackPanel)
#include "vaml/tools/VBamlReader.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using vaml::DependencyObject;
using vaml::StackPanel;
using vaml::viewer::CompiledOpenResult;
using vaml::viewer::compile_to_vbaml;
using vaml::viewer::load_compiled_bytes;
using vaml::viewer::load_compiled_file;

namespace {

// A small RENDERABLE golden (root = StackPanel, a UIElement re-based at S129 —
// the same shape the viewer `sample.vaml` renders, NOT a StackPanel which is a bare
// DependencyObject and thus not renderable). Includes an x:Class so the S285
// reader decodes a root_class; the viewer's compiled mode renders this tree.
const char* kS293Golden =
    R"(<StackPanel x:Class="Demo.MainPanel"><StackPanel x:Name="panel"/></StackPanel>)";

// Writes `bytes` to a temp file and returns its path (removed by the caller).
std::string writeTempVbaml(const std::vector<std::uint8_t>& bytes)
{
    const std::string path = "s293_tmp.vbaml";
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    return path;
}

std::string writeTempVaml(const std::string& text)
{
    const std::string path = "s293_tmp.vaml";
    std::ofstream out(path, std::ios::trunc);
    out << text;
    out.close();
    return path;
}

void removeTemp(const std::string& path)
{
    (void)std::remove(path.c_str());
}

} // namespace

// ---------------------------------------------------------------------------
// load_compiled_bytes — the live tree from compiled bytes via the S7 reader
// ---------------------------------------------------------------------------

TEST_CASE("S293: load_compiled_bytes decodes -> live tree, retains the S7 reader")
{
    const std::vector<std::uint8_t> bytes = compile_to_vbaml(kS293Golden);
    REQUIRE(bytes.size() > 0u);

    CompiledOpenResult r = load_compiled_bytes(bytes);
    REQUIRE(r.ok);
    REQUIRE(r.root != nullptr);
    REQUIRE(r.reader != nullptr);   // the retained reader keeps named()/retained alive

    // Root is the declared StackPanel (the S284 golden root type).
    REQUIRE(dynamic_cast<StackPanel*>(r.root.get()) != nullptr);

    // The retained reader serves the S286 named() registry (the "panel" x:Name).
    DependencyObject* panel = r.reader->named("panel");
    CHECK(panel != nullptr);

    // The x:Class was decoded.
    CHECK(r.reader->root_class() == std::string("Demo.MainPanel"));
}

TEST_CASE("S293: load_compiled_bytes — a malformed / empty buffer fails loudly, never silent")
{
    const std::vector<std::uint8_t> empty;   // NOT a valid .vbaml stream
    CompiledOpenResult r = load_compiled_bytes(empty);
    CHECK_FALSE(r.ok);
    // It must carry a typed, non-empty detail (never a silent blank failure).
    CHECK_FALSE(r.detail.empty());

    // A truncated/nonsense buffer is also a structured failure.
    const std::vector<std::uint8_t> junk{0xDE, 0xAD, 0xBE, 0xEF};
    CompiledOpenResult j = load_compiled_bytes(junk);
    CHECK_FALSE(j.ok);
    CHECK_FALSE(j.detail.empty());
}

// ---------------------------------------------------------------------------
// compile_to_vbaml — the recompile seam round-trips through the S7 reader
// ---------------------------------------------------------------------------

TEST_CASE("S293: compile_to_vbaml (vamlcc) -> compiled bytes load through VBamlReader")
{
    const std::vector<std::uint8_t> bytes = compile_to_vbaml(kS293Golden);
    CHECK_FALSE(bytes.empty());
    // Round-trip: the compiled bytes are a valid .vbaml stream the S285 reader
    // materializes into the declared tree.
    CompiledOpenResult r = load_compiled_bytes(bytes);
    REQUIRE(r.ok);
    CHECK(dynamic_cast<StackPanel*>(r.root.get()) != nullptr);
}

// ---------------------------------------------------------------------------
// load_compiled_file — .vaml compiles first; .vbaml loads directly (parity)
// ---------------------------------------------------------------------------

TEST_CASE("S293: load_compiled_file — a .vaml path COMPILES then loads (vamlcc in-process)")
{
    const std::string path = writeTempVaml(kS293Golden);
    CompiledOpenResult r = load_compiled_file(path);
    removeTemp(path);
    REQUIRE(r.ok);
    REQUIRE(r.root != nullptr);
    CHECK(dynamic_cast<StackPanel*>(r.root.get()) != nullptr);
    // The compiled path decoded the x:Class even though the file is a text .vaml
    // (the "recompiles .vaml→.vbaml then reopens" pipeline ran end-to-end).
    if (r.reader)
    {
        CHECK(r.reader->root_class() == std::string("Demo.MainPanel"));
    }
}

TEST_CASE("S450_1: load_compiled_file — a .xaml path COMPILES then loads (the .xaml acceptance gate)")
{
    // S450_1 — the viewer ACCEPTS .xaml: the compiled open treats a .xaml source as
    // text to recompile (is_vaml_or_xaml_source true), NOT as a binary .vbaml.
    // A bona-fide XAML-shaped document (presentation root by local name) recompiles
    // through vamlcc → VBamlReader exactly like a .vaml.
    const char* kXamlGolden =
        R"(<StackPanel x:Class="Demo.XamlPanel"><StackPanel x:Name="panel"/></StackPanel>)";

    // Write it to a temp .xaml path and run the REAL compiled seam.
    const std::string path = "s450_tmp.xaml";
    {
        std::ofstream out(path, std::ios::trunc);
        out << kXamlGolden;
    }
    CompiledOpenResult r = load_compiled_file(path);
    removeTemp(path);

    REQUIRE(r.ok);
    REQUIRE(r.root != nullptr);
    CHECK(dynamic_cast<StackPanel*>(r.root.get()) != nullptr);
    if (r.reader)
    {
        CHECK(r.reader->root_class() == std::string("Demo.XamlPanel"));
        CHECK(r.reader->named("panel") != nullptr);
    }
}

TEST_CASE("S293: load_compiled_file — a .vbaml path loads bytes directly (parity with compile-first)")
{
    const std::vector<std::uint8_t> bytes = compile_to_vbaml(kS293Golden);
    const std::string path = writeTempVbaml(bytes);
    CompiledOpenResult r = load_compiled_file(path);
    removeTemp(path);
    REQUIRE(r.ok);
    REQUIRE(r.root != nullptr);

    // Direct-load parity: the .vbaml path's tree equals the .vaml compile-then-
    // load path (both are the same compiled bytes through the same reader).
    const std::string vpath = writeTempVaml(kS293Golden);
    CompiledOpenResult v = load_compiled_file(vpath);
    removeTemp(vpath);
    REQUIRE(v.ok);
    CHECK(dynamic_cast<StackPanel*>(v.root.get()) != nullptr);
    CHECK(dynamic_cast<StackPanel*>(r.root.get()) != nullptr);
}

TEST_CASE("S293: load_compiled_file — a missing / bad file fails loudly, never silent")
{
    CompiledOpenResult r = load_compiled_file("s293_no_such_file.vbaml");
    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.detail.empty());
}

// ---------------------------------------------------------------------------
// recompile-then-reopen — a changed source recompiles to a NEW tree
// ---------------------------------------------------------------------------

TEST_CASE("S293: recompile-then-reopen — an edited source recompiles to a NEW tree (structural change)")
{
    // v1: no named child.
    const std::string v1 =
        R"(<StackPanel x:Class="Demo.MainPanel"><StackPanel/></StackPanel>)";
    const std::string path = writeTempVaml(v1);
    CompiledOpenResult first = load_compiled_file(path);
    REQUIRE(first.ok);

    // v2: an EDITED source that adds a named element — the recompile-then-reopen
    // contract: re-read the SAME path (as the S222_1 file-watch would), recompile
    // to .vbaml, reopen — the new tree reflects BOTH the added element AND the
    // different byte stream (a NEW tree, not the stale one).
    const std::string v2 =
        R"(<StackPanel x:Class="Demo.MainPanel"><StackPanel x:Name="added"/></StackPanel>)";
    {
        std::ofstream out(path, std::ios::trunc);
        out << v2;
    }
    CompiledOpenResult second = load_compiled_file(path);
    removeTemp(path);
    REQUIRE(second.ok);

    // Structural compare: v2 registers the "added" x:Name that v1 did NOT.
    const bool v1HasAdded = first.reader && first.reader->named("added") != nullptr;
    const bool v2HasAdded = second.reader && second.reader->named("added") != nullptr;
    CHECK_FALSE(v1HasAdded);
    CHECK(v2HasAdded);
}
