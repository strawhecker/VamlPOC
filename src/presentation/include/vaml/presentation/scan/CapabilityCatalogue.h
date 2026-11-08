#pragma once
// S642 — GENERIC CAPABILITY PARITY — CapabilityCatalogue.h (the LIB side of the
// typed capability catalogue both sides implement IDENTICALLY).
//
// This is the C++ half of the single-source-of-truth catalogue that lives as
// `tools/capability_catalogue.json` (the SAME file the WPF-emitter C# side + the
// PARITY MATRIX generator read). The catalogue is the "keep both in sync"
// mechanism: every capability (typed slot / icon / text) carries a SYNC TOKEN —
// it may only be introduced by updating OUR side AND the WPF-emitter side AND the
// matrix IN THE SAME STEP (never one-sided, never silent).
//
// This header exposes the C++-side catalogue so the scanner's tolerance scaffold
// and the compare are CATALOGUE-DRIVEN (the S641 `default_tolerance_for` built-in
// becomes a catalogue read), and so a headless ctest asserts parity (our-side ==
// WPF-side == matrix) for every capability — the enforceable half of the sync
// token (the C# side commits to the SAME committed JSON; its CI-half is the
// committed fixture + the C++ reader test).
//
// The catalogue JSON is embedded at compile time (no runtime file dependency —
// the same discipline as the S165 embedded-SPIR-V / S203 embedded-font precedent)
// so BOTH CI lanes run the exact same catalogue without a filesystem path.
//
// ZERO LOCKS (ADR-018) + ZERO MINIMAL REAL (permanent, kept-and-extended surface).

#include <vaml/presentation/scan/ScanDeclaration.h>  // QuadKind reuse (Solid/Text/Icon)

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef VAML_PRESENTATION_EXPORT
#  define VAML_PRESENTATION_EXPORT
#endif

namespace vaml::presentation::scan {

// The status tri-state a capability's OUR side / WPF side can be in (the status
// column of the PARITY MATRIX). `Introduced` = authored/declared at a step;
// `Done` = fully real; `Planned` = scheduled; `OutOfScope` = declared not-silent.
enum class CapabilityStatus
{
    Introduced,
    Done,
    Planned,
    OutOfScope,
};

// One capability row of the catalogue. The fields mirror the JSON row + the
// PARITY MATRIX columns exactly (no drift).
struct Capability
{
    std::string  id;          // e.g. "slot_int_01", "icon_01", "text_01"
    std::string  family;      // e.g. "slot_int", "icon", "text"
    std::string  type;        // "int" | "bool" | "string" | "double" | "icon" | "text"
    QuadKind     kind      = QuadKind::Solid;   // the scanner diff kind
    double       tolerance = 0.02;               // the per-capability default
    CapabilityStatus our     = CapabilityStatus::Introduced;
    CapabilityStatus wpf     = CapabilityStatus::Introduced;
    bool         parity_yes = true;             // parity yes/no
    std::string  gap;                            // named gap (never empty on parity=no)
};

// The whole catalogue: the family metadata (soft-start counts) + the ordered
// capability list.
struct CapabilityCatalogue
{
    int version = 1;
    bool counts_are_soft_start = true;
    std::vector<Capability> capabilities;
};

// Parses the embedded catalogue JSON text into a CapabilityCatalogue. Returns
// false (never silent) on any malformed/blank field. PURE + headless.
VAML_PRESENTATION_EXPORT CapabilityCatalogue parse_capability_catalogue(const std::string& jsonText);

// The catalogue loaded from the embedded JSON (compile-time single source of truth).
// Returns true when a catalogue is available (the embedded bytes parse).
VAML_PRESENTATION_EXPORT const CapabilityCatalogue& capability_catalogue() noexcept;
// True when capability_catalogue() parsed successfully (not a blob-empty stub).
VAML_PRESENTATION_EXPORT bool capability_catalogue_loaded() noexcept;

// Looks up a capability by id; returns nullptr when absent.
VAML_PRESENTATION_EXPORT const Capability* find_capability(const CapabilityCatalogue& cat,
                                                           const std::string& id) noexcept;

// The default tolerance for a capability (falling back to the built-in kind
// default — the S641 scaffold) — a catalogued capability's declared tolerance
// WINS. PURE.
VAML_PRESENTATION_EXPORT double catalogue_tolerance_for(const Capability& cap) noexcept;

// Family counts (soft-start). Returns the current count of a named family (0 when
// absent).
VAML_PRESENTATION_EXPORT std::size_t family_count(const CapabilityCatalogue& cat,
                                                   const std::string& family) noexcept;

// The typed-slot/icon/text capability summary (for the headless parity test): the
// total capability count + per-family counts.
struct CapabilitySummary
{
    std::size_t total     = 0;
    std::size_t ints      = 0;
    std::size_t bools     = 0;
    std::size_t strings   = 0;
    std::size_t doubles   = 0;
    std::size_t icons     = 0;
    std::size_t texts     = 0;
};
VAML_PRESENTATION_EXPORT CapabilitySummary summarize(const CapabilityCatalogue& cat) noexcept;

} // namespace vaml::presentation::scan
