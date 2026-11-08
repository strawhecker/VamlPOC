#pragma once
// S641 — THE UI-COMPLIANCE SCANNER — EmitterMetadata.h (forward-compatible sidecar reader).
//
// The compare consumes the emitter-deposited metadata (quad kinds + tolerance)
// from the sidecar (`out.json`). This header reads that JSON — the schema the
// S640b WPF emitter writes:
//
//   {
//     "Width": 440, "Height": 240, "Source": "x.xaml",
//     "Quads": [ { "Name": "SampleText", "X": 16, "Y": 96,
//                  "Width": 120, "Height": 40, "Kind": "Text" }, ... ]
//   }
//
// METADATA READ (baked): S641 READS whatever the emitter deposits TODAY; even if
// S642 refines the metadata shape, S641 reads the current shape — the reader is
// FORWARD-COMPATIBLE (unknown/extra fields are IGNORED), so a later gap-close
// never strands it. The in-XAML `SCAN DECLARATION` remains the PRIMARY oracle;
// the sidecar is the secondary, per-quad-kind oracle.
//
// Hand-rolled, focused JSON scanner (no external JSON dep — the repo vendors
// none for scanning; a tiny state machine is enough for this fixed schema).
// PURE + headless (both CI lanes).
//
// ZERO LOCKS (ADR-018) + ZERO MINIMAL REAL.

#include <vaml/presentation/scan/ScanDeclaration.h> // QuadKind enum reuse

#include <cstdint>
#include <string>
#include <vector>

#ifndef VAML_PRESENTATION_EXPORT
#  define VAML_PRESENTATION_EXPORT
#endif

namespace vaml::presentation::scan {

// A per-quad entry read from the sidecar (only the fields S641 consumes).
struct MetadataQuad
{
    std::string name;
    int x = 0, y = 0, w = 0, h = 0;
    QuadKind kind = QuadKind::Solid; // parsed from the string "Solid"/"Text"/"Icon"
    bool kind_known = false;
};

// The parsed sidecar.
struct EmitterMetadata
{
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
    std::string source;
    std::vector<MetadataQuad> quads;
};

// The typed JSON-read error (never silent).
enum class MetadataError
{
    None,
    NotObject,       // the root is not a JSON object
    BadNumber,       // a numeric field could not be read
    BadArray,        // Quads is not an array
    BadQuad,         // an entry is not an object / shape wrong
    Truncated,       // the JSON text is malformed/incomplete
};

struct MetadataResult
{
    bool ok = false;
    EmitterMetadata metadata;
    MetadataError error = MetadataError::None;
    std::string detail;
};

// Parses the sidecar JSON text into EmitterMetadata. FORWARD-COMPATIBLE:
// unknown/extra fields are IGNORED (a later S642 shape change never strands
// this reader). PURE + headless.
VAML_PRESENTATION_EXPORT MetadataResult parse_emitter_metadata(const std::string& jsonText);

// Convenience: reads + parses a `.json` file path. Never silent on I/O failure.
VAML_PRESENTATION_EXPORT MetadataResult read_emitter_metadata_file(const std::string& path);

} // namespace vaml::presentation::scan
