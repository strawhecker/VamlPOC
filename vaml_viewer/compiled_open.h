// vaml_viewer — the S293 VIEWER-GROWTH compiled-path seam (dep:S288; the
// "compiled .vbaml" viewer-growth token).
//
// THE "OPEN A COMPILED .vbaml VIA THE RUNTIME VBamlReader (S285)" SEAM. The
// viewer's compiled mode consumes the S7 reader (vaml::tools::VBamlReader) to
// materialize a live tree from a COMPILED `.vbaml` byte stream — the
// text→compiled→render path demonstrated in the live viewer. This is the
// S280→S289 (text → vamlcc → .vbaml → vaml-tools) pipeline made visible.
//
// THREE REAL seams (headless-testable, ADR-021 consumer pattern — compiled into
// both apps + the test target, the standing S221/S223 precedent):
//   * `load_compiled_bytes(bytes)` — a live tree from compiled bytes via the
//     REAL VBamlReader (S285 Load → tree). Returns { ok, root, retained reader,
//     detail }. A malformed `.vbaml` is a typed, never-silent failure.
//   * `compile_to_vbaml(text)` — the recompile seam over the REAL
//     `vaml::tools::compile_to_bytes` (in-process; NO subprocess — the ADR-021
//     headless discipline; the Dev-tool vamlcc path reused, not re-shelled).
//   * `load_compiled_file(path)` — reads bytes; a `.vaml` path COMPILES first
//     (compile_to_vbaml) then loads via VBamlReader; a `.vbaml` path loads the
//     bytes directly. Structured union of FileLoadError + VBamlException.
//
// The retained reader is returned owned (heap) so the S286 deferred extensions
// + named() registry stay alive with the tree (mirrors the S256 viewer_binding
// plan's retained-reader pattern). The S7 reader defers {Binding} (S286) — the
// compiled tree renders the real panels, with binding-on-compiled deferred to
// the S444/S322 release path (recorded NO-GAP, not silent).
//
// NO MINIMAL REAL introduced — every seam is REAL permanent code, kept-and-
// extended by the S444 app-shell. ADR-018: ZERO new locks (single-threaded on
// the viewer's UI thread).

#pragma once

#include "vaml/core/DependencyObject.h"   // the owned root
#include "vaml/tools/VBamlReader.h"       // vaml::tools::VBamlReader (S7 reader)
#include "vaml/tools/Vamlc.h"             // vaml::tools::compile_to_bytes (vamlcc S283)

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace vaml::viewer {

// The outcome of a compiled open.
//
// `reader` is owned on the HEAP and returned so the S286 deferred surfaces +
// named() registry stay alive alongside `root` (the S256 retained-reader
// pattern — the reader holds the non-root retained objects, so the tree stays
// valid for as long as the caller holds both).
struct CompiledOpenResult
{
    bool ok = false;                              // bytes decoded + loaded cleanly
    std::unique_ptr<vaml::DependencyObject> root; // the live tree (UIElement root)
    std::unique_ptr<vaml::tools::VBamlReader> reader; // RETAINED S7 reader (keeps named()/retained alive)
    std::string detail;                           // the structured failure reason (never silent)
};

// THE REAL COMPILED-BYTES → TREE SEAM. Feeds the compiled bytes to the S285
// VBamlReader (the S7 runtime reader — the exact path the release app-shell
// uses). A malformed `.vbaml` throws VBamlException; it is mapped here to a
// typed, never-silent failure detail. The reader is returned owned so its
// retained non-root objects (which the tree borrows via parent links) survive.
//
// `events` (optional) — the reader-level EVENT set to register BEFORE Load
// (the S288/S443 reader contract: an event-shaped key like Click="OnClick"
// routes to a deferred event handler ONLY when the event name is registered;
// otherwise it is an unknown-property throw). S444 passes {"Click"} for the
// app-shell; the S293 callers pass none (their goldens have no events).
[[nodiscard]] inline CompiledOpenResult
load_compiled_bytes(const std::vector<std::uint8_t>& bytes,
                    const std::vector<std::string>* events = nullptr)
{
    CompiledOpenResult r;
    try
    {
        r.reader = std::make_unique<vaml::tools::VBamlReader>();
        if (events)
        {
            for (const std::string& ev : *events)
            {
                r.reader->register_event(ev);
            }
        }
        r.root = r.reader->Load(bytes);   // S285: bytes → live DependencyObject tree
        r.ok = (r.root != nullptr);
    }
    catch (const std::exception& e)
    {
        r.ok = false;
        r.detail = e.what();              // never silent
    }
    return r;
}

// THE RECOMPILE SEAM (the "reload recompiles .vaml→.vbaml then reopens"
// headline). Runs the REAL vamlcc compiler in-process
// (vaml::tools::compile_to_bytes, S283) — the same Dev-tool compiler the CLI
// runs, reused rather than re-shelled (ADR-021 headless discipline; a
// subprocess would break the shared-seam testability). Returns the compiled
// bytes, or an EMPTY vector on a compile error (the caller reports the detail
// from CompileResult — see the file variant which carries it).
[[nodiscard]] inline std::vector<std::uint8_t> compile_to_vbaml(const std::string& text)
{
    return vaml::tools::compile_to_bytes(text);
}

namespace detail {

// True when `path` ends with the case-insensitive `.vbaml` extension.
inline bool is_vbaml_path(const std::string& path) noexcept
{
    const std::string needle = ".vbaml";
    if (path.size() < needle.size())
    {
        return false;
    }
    const std::size_t off = path.size() - needle.size();
    for (std::size_t i = 0; i < needle.size(); ++i)
    {
        const char a = path[off + i];
        const char b = needle[i];
        const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
        const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
        if (la != lb)
        {
            return false;
        }
    }
    return true;
}

} // namespace detail

// THE FILE SEAM. Reads `path` and produces a live compiled tree:
//   * a `.vbaml` path  → reads the bytes → load_compiled_bytes.
//   * a `.vaml` path   → reads the TEXT → compile_to_vbaml (vamlcc in-process)
//                        → load_compiled_bytes (the "recompiles .vaml→.vbaml
//                        then reopens" path).
// `events` (optional) — forwarded to load_compiled_bytes (S444 app-shell
// passes {"Click"}). The outcome detail carries a structured, never-silent
// reason on failure.
[[nodiscard]] inline CompiledOpenResult load_compiled_file(const std::string& path,
                                                           const std::vector<std::string>* events = nullptr)
{
    // Read the raw bytes (works for both .vaml text and .vbaml binary).
    {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
        {
            CompiledOpenResult r;
            r.ok = false;
            r.detail = "no such file: '" + path + "'";
            return r;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        if (in.bad())
        {
            CompiledOpenResult r;
            r.ok = false;
            r.detail = "cannot read file: '" + path + "'";
            return r;
        }
        if (detail::is_vbaml_path(path))
        {
            const std::string data = ss.str();
            std::vector<std::uint8_t> bytes(data.begin(), data.end());
            return load_compiled_bytes(bytes, events);
        }
        // A `.vaml` (or extensionless) source: recompile to .vbaml first.
        const std::string text = ss.str();
        std::vector<std::uint8_t> bytes;
        try
        {
            bytes = compile_to_vbaml(text);
        }
        catch (const std::exception& e)
        {
            CompiledOpenResult r;
            r.ok = false;
            r.detail = "compile failed: " + std::string(e.what());
            return r;
        }
        if (bytes.empty())
        {
            CompiledOpenResult r;
            r.ok = false;
            r.detail = "compile produced no bytes for '" + path + "'";
            return r;
        }
        return load_compiled_bytes(bytes, events);
    }
}

} // namespace vaml::viewer
