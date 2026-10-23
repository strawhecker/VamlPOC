// vaml_viewer — load_file seam (S221, dep:S220).
//
// THE HEADLESS-ctestable FILE→TREE ENTRY PATH. S221's contract: `vaml_viewer
// <file.vaml>` opens a file, runs VamlReader::Load (the S091/S092 public
// surface) and exposes the resulting live tree — OR a TYPED, structured error
// on missing / unreadable / parse-error (NEVER silent). This file is the
// single, platform-independent seam that keeps that entry path REAL and
// testable without a live window:
//
//   * It is compiled by the Windows VS project (vaml_viewer/), the Linux CMake
//     project (vaml_viewer-linux/), AND the `vaml-core-tests` target — ONE
//     source of truth for the entry path.
//   * The tests drive it headlessly (load_text / load_file against a golden
//     VAML fixture), so the exact code the CLI runs is the exact code CI
//     verifies. No drift between app and test.
//
// CONSUMER-APP PATTERN (ADR-021) maintained: this seam includes ONLY public
// library headers (<vaml/...> from the public include roots) and never reaches
// into a library's private implementation. It is the genuine ADR-021 consumer
// path, compiled into the test target so the headless suite exercises it as a
// real consumer.
//
// SCOPE BOUNDARY (recorded, NOT silent): `load_file`/`load_text` only READ +
// VamlReader::Load — they build the in-memory live tree, they do NOT render
// (rendering = S222 "soup-to-nuts"; pan/zoom = S224). The viewer CLI wiring
// (argv → seam → exit code) lives in main.cpp, never in this seam and never in
// the tests.
#pragma once

#include <memory>
#include <string>

// The seam's LoadResult owns a std::unique_ptr<DependencyObject>, whose
// destructor/copy-move need the COMPLETE type at the point of use (main.cpp
// and the tests consume LoadResult by value). Include the public header (the
// ADR-021 consumer path, never an internal header) so the unique_ptr members
// are valid everywhere the seam is used.
#include "vaml/core/DependencyObject.h"

namespace vaml {

// The 3-way open outcome. Decomposes the "never silent" failure into exactly
// what an operator / test needs to distinguish:
//   * NoSuchFile  — the path does not exist / could not be opened.
//   * ReadError   — the path opened but could not be fully read.
//   * ParseError  — the file READ fine but VamlReader::Load rejected it
//                   (structured scan/parse/resolution/conversion failure).
enum class FileLoadError
{
    None = 0,
    NoSuchFile,
    ReadError,
    ParseError,
};

// The structured result of a file→tree load. `ok` is true iff `root` holds a
// live tree and `error == FileLoadError::None`. On failure `error` names the
// category and `detail` carries the underlying structured message (the
// exception's what() — never a bare silence). `root` is null on failure.
struct LoadResult
{
    bool ok = false;
    std::unique_ptr<DependencyObject> root;
    FileLoadError error = FileLoadError::None;
    std::string detail;
};

// Reads `path` and Loads a live tree. File I/O failures become NoSuchFile /
// ReadError (never silent, always categorized); document-level failures
// (XmlScanException / XmlParseException / VamlReaderException /
// ElementResolutionException / ValueConversionException) become ParseError
// with the structured detail. Equivalent to read-then-load_text.
[[nodiscard]] LoadResult load_file(const std::string& path);

// Loads a live tree from an already-read text buffer. The PURE seam — converts
// VamlReader's structured exception taxonomy into the typed LoadResult. The
// golden-fixture tests target THIS (deterministic in-memory injection), and
// load_file delegates its read bytes here (DRY parse-error mapping).
[[nodiscard]] LoadResult load_text(const std::string& text);

} // namespace vaml
