#pragma once
// S641 — THE UI-COMPLIANCE SCANNER — in-XAML `SCAN DECLARATION` parser.
//
// Sprint 11r (S641, dep:S640b): the root capability the whole sprint rides on.
// The compare seam (the compare API `compliance_compare(actualScan,
// expectedDecl)`, the tolerance scaffold, and the in-XAML `SCAN DECLARATION`
// parser) lives in the DEDICATED tester. This header is the PURE in-XAML
// declaration reader: an authored `.xaml` DECLARES its expected outcome inside
// a `<!-- SCAN DECLARATION ... END SCAN DECLARATION -->` comment block, and the
// scanner reads THAT declaration (the primary oracle) to know WHERE + WHAT to
// scrape + compare.
//
// PER-QUAD DECLARATION (baked from the roadmap): every S641+ test encodes its
// quads WITH a `tolerance` OR a `handling text|icon` in the declaration —
// solid geometry -> tight tolerance; text -> auto-locate + tolerance band;
// icon -> presence/centroid + loose. The compare is per-quad configurable,
// NEVER a silent global bit-match. Malformed declaration -> a TYPED structured
// parse error (never silent).
//
// GRAMMAR (hand-rolled raw-file comment-block reader; NO regex, NO XML dep):
//   <!-- SCAN DECLARATION
//     frame 440 360                      ; authoritative render size
//     quad  img   solid  rect 32,32,128,128   tolerance 0.10
//     quad  title text   auto                band 16  expect-literal "Hello"
//     quad  ok    icon   auto                centroid tolerance 0.25
//   END SCAN DECLARATION -->
//
//   frame W H                    — authoritative render width/height.
//   quad NAME KIND rect X,Y,W,H [tolerance T] [capability ID]
//                                — solid geometry at a declared rect (+ optional tight tolerance).
//   quad NAME KIND auto [band B] [expect-literal "SUB"] [tolerance T] [capability ID]
//                                — text: auto-locate + tolerance band (+ optional declared substring).
//   quad NAME KIND auto [centroid] [presence] [tolerance T] [capability ID]
//                                — icon: auto-locate + presence/centroid + loose tolerance.
//
//   KIND = solid | text | icon. `tolerance` is a per-quad jointolerance override
//   (a 0..1 fraction); when absent the compare merges the emitter-sidecar kind
//   -> built-in kind default (the tolerance scaffold). `auto` vs `rect` selects
//   auto-locate vs a declared frame rect. Unknown/typo'd keywords or bad numbers
//   -> DeclarationError with a line + detail (never silent).
//
// ZERO LOCKS (ADR-018): single-threaded pure read. ZERO MINIMAL REAL — everything
// here is permanent, kept-and-extended ADR-021 surface.

#include <cstdint>
#include <string>
#include <vector>

#ifndef VAML_PRESENTATION_EXPORT
#  define VAML_PRESENTATION_EXPORT
#endif

namespace vaml::presentation::scan {

// The per-quad diff kind (mirrors the emitter sidecar's QuadKind enum — the
// S641 tolerance scaffold keys off it). `Solid` = geometry -> tight tolerance;
// `Text` = auto-locate + band; `Icon` = presence/centroid + loose.
enum class QuadKind
{
    Solid,
    Text,
    Icon,
};

// The built-in per-kind default jointolerance (the tolerance scaffold's floor).
// `Solid` geometry is tight; `Icon` is loose (centroid/presence);
// `Text` sits between (auto-locate band). Exposed + headless-testable.
VAML_PRESENTATION_EXPORT double default_tolerance_for(QuadKind kind) noexcept;

// ONE declared quad: what the compare must verify about ONE named region.
// `located` = true when the author declared an explicit frame rect (solid
// geometry); false = auto-locate (text/icon). `rect` carries the declared
// frame rect when located. `has_tolerance`/`tolerance` is the per-quad override.
struct ScanQuad
{
    std::string name;
    QuadKind    kind        = QuadKind::Solid;
    bool        located     = false;   // false = auto-locate (text/icon)
    float       x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;  // declared frame rect (when located)
    bool        has_tolerance = false; // per-quad declared jointolerance override
    double      tolerance     = 0.0;   // declared override (when has_tolerance)

    // --- text (auto-locate) ---
    int         band             = 0;  // tolerance band (px) around auto-located text
    std::string expect_literal;        // declared substring (informational/human aid; NOT pixel OCR)

    // --- icon (auto-locate) ---
    bool        require_presence  = false; // element/presence check
    bool        require_centroid  = false; // centroid-within-tolerance check

    // S642 (provisional metadata fine-tune): the SYNC-TOKEN capability id into the
    // capability catalogue (e.g. "text_01", "icon_02"). The in-XAML declaration may
    // optionally tag a quad with `capability <id>` so the author's intent + the
    // emitter sidecar + the PARITY MATRIX all key the SAME catalogue row (the "keep
    // both in sync" mechanism). Empty = the quad is not catalogue-backed. Provisional
    // until S643's screens prove the quads carry the tolerance info the compare needs.
    std::string capability;
};

// The whole declaration: the authoritative render frame + the ordered quad set.
struct ScanDeclaration
{
    std::uint32_t width  = 0;   // `frame W`
    std::uint32_t height = 0;   // `frame H`
    std::vector<ScanQuad> quads;
};

// The typed parse-error taxonomy (never silent). The parser never swallows a
// malformed declaration.
enum class DeclarationError
{
    None,
    NoBlock,        // no `<!-- SCAN DECLARATION ... END SCAN DECLARATION -->` block found
    MalformedLine,  // a token/line could not be understood
    UnknownKeyword, // an unrecognized directive token
    BadNumber,      // a numeric field was not a valid positive number
    BadRect,        // a `rect` had the wrong arity or non-positive size
    BadKind,        // the KIND token was not solid|text|icon
    MissingEnd,     // the block opened but never hit END SCAN DECLARATION
};

// The parse result: on success `ok` + a populated `declaration`; on failure the
// typed `error` + a ONE-LINE `detail` naming the offending line content +
// `line` (1-based; 0 when no line applies). NEVER silent.
struct DeclarationResult
{
    bool ok = false;
    ScanDeclaration declaration;
    DeclarationError error = DeclarationError::None;
    std::string detail;   // human-readable (never empty on !ok)
    int line = 0;         // 1-based offending line (0 = not line-specific)
};

// Parses the in-XAML `SCAN DECLARATION` block out of the raw `.xaml` text.
// PURE + headless (both CI lanes). No regex/XML dependency — a hand-rolled
// comment-block + line/token scan (the S366/S452 grammar precedent).
VAML_PRESENTATION_EXPORT DeclarationResult parse_scan_declaration(const std::string& xamlText);

} // namespace vaml::presentation::scan
