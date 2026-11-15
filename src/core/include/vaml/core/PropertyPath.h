#pragma once
// VAML PropertyPath — the parsed path binding route + its evaluator.
// S240 (dep:S226). Sprint 6 first token. Foundation for the binding engine.
//
// This is REAL permanent core surface (NOT a STUB / NOT MINIMAL REAL — there
// is no deferred backfill; the type is complete on delivery) and is headless-
// testable on the `vaml::Value` + structured-error + Shouldly conventions
// established by S030..S208.
//
// Two distinct, deliberately-split responsibility classes (this split is the
// S240_1/BindingDiagnostic precondition — recorded, NOT silent):
//   - Class A (syntax / authoring): PropertyPathError's TrailingDot,
//     ConsecutiveDot, UnterminatedIndexer, EmptyIndexer, MalformedIndex,
//     UnclosedQuote, UnmatchedParen — thrown by parse() as
//     PropertyPathException. Static: the path TEXT is wrong and no data is
//     needed to know it.
//   - Class B (evaluation / runtime data outcome): NullIntermediate,
//     MissingProperty, NotReadable, Direct — produced by the EVALUATOR (never
//     by parse()). They depend on the live data, so they are reported back as
//     PropertyRead outcomes, not thrown diagnostics.
//
// Grammar (hand-rolled, WPF-faithful — audited against
// E:\GitRepos\wpf\...\MS\Internal\Data\PathParser.cs per the WPF-source-audit
// protocol): a path is a sequence of route steps, each a property name or an
// indexer. The FIRST level may start bare (`Address`, `[0]`, `Address[0].Name`),
// OR with a leading `.` (`DrillIn.Never`), `/` (`DrillIn.Always`), or `[` — the
// WPF Init/DrillIn state machine (PathParser.cs:103-136). An EMPTY / `.` path
// is a VALID `Direct` binding to the whole object (PathParser.cs:83-89), NOT an
// error. A name is a run excluding `.` `/` `[` `]` (IsSpecialChar, :401-405) —
// internal whitespace is legal (names are trimmed, :150-165); `( )` paren-level
// tracking is honored (:144-149). An indexer is a comma-separated
// IndexerParamInfo{parenString, valueString} list (:17-32, :172-181) supporting
// a `(paren)value` form, multi-param `[i,j]`, nested `[ [ ] ]`, and the `^`
// escape of `, ( ) ] ^` and white space (EscapeChar = '^', :417, :239-247).
//
// The parser is hand-rolled (no std::regex, no parser-generator dependency):
// the path grammar is small and deterministic, and hand-rolling keeps this
// core surface dependency-free (ADR-011 clean, no Vulkan; no runtime libs).

#include "vaml/core/Value.h" // Value (evaluator's payload), ValueType

#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// --- Class A + Class B outcome kinds (uint8_t so it can cross thread/queue
// boundaries trivially and be stored compactly in diagnostics). ----------
enum class PropertyPathError : std::uint8_t
{
    // Class A — syntax (parse-time, authoring). parse() throws these.
    // NOTE: `Empty` is RETIRED as a syntax error (the WPF drift closure at
    // Sprint 11af): an empty / `.` path is a VALID `Direct` self-binding
    // (PathParser.cs:83-89), so the enum keeps `Empty = 0` as the stable
    // "no-error / ok" tag used by PropertyRead and diagnostics, and the Class A
    // error set is the remaining syntax arms. The order below is STABLE (the
    // surviving error ordinals are unchanged; `UnmatchedParen` is appended).
    Empty             = 0,  // no-error/ok tag (a Direct path is valid, not an error)
    TrailingDot       = 1,  // path ends mid-language-gate (e.g. "...Address/")
    ConsecutiveDot    = 2,  // "Address..Street" debug-only remnant — real WPF treats
                            // `.` as DrillIn.Never and a leading `.` is legal; kept
                            // as a stable ordinal, reported only for the archived form
    UnterminatedIndexer = 3,// "Items[0"        (bracket never closed)
    EmptyIndexer      = 4,  // "Items[]"        (no index inside brackets)
    MalformedIndex    = 5,  // "Items[abc]" / "Items[1.5]" / "Items[-]" (bad index)
    UnclosedQuote     = 6,  // "Items[\"abc]    (quote never closed)
    UnmatchedParen    = 10, // "(Name" / "Name)" unbalanced paren in a property name
                            // (the WPF UnmatchedParen error, PathParser.cs:144-149)

    // Class B — evaluation (runtime data outcome, never thrown by parse()).
    NullIntermediate  = 7,  // a NON-final segment yielded Value::Null
    MissingProperty   = 8,  // the read resolved to an unknown property
    NotReadable       = 9,  // the node cannot be read by this reader (e.g. an
                            // index into a non-indexable DependencyObject)
    Direct            = 11, // the path was empty/`.` — bind the whole object
                            // (equivalent to WPF SourceValueInfo(Direct), :83-89)
};

// The three drill-in modes WPF's parser tracks per level
// (PathParser.cs:17-32, :80, :372).
enum class DrillIn : std::uint8_t
{
    Never    = 0, // '.' — property access, no drill-in
    IfNeeded = 1, // first-level default
    Always   = 2, // '/' — drill into the collection/items
};

// Structured parse error (the S082/S091/S094 structured-exception pattern):
// kind(), path() and a human-readable what() that NAMES THE AUTHORED FRAGMENT
// that produced the error. Throw by PropertyPath::parse on any Class A kind.
class VAML_CORE_EXPORT PropertyPathException : public std::logic_error
{
public:
    PropertyPathException(PropertyPathError kind, std::string path,
                          std::string fragment);

    [[nodiscard]] PropertyPathError kind() const noexcept { return kind_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }
    // The exact authored sub-text responsible (e.g. "Address..", "Items[abc]",
    // "\"abc"). Present so diagnostics can pinpoint the culprit.
    [[nodiscard]] const std::string& fragment() const noexcept { return fragment_; }

private:
    PropertyPathError kind_;
    std::string       path_;
    std::string       fragment_;
};

// One indexer parameter, the WPF `IndexerParamInfo{parenString, valueString}`
// unit (PathParser.cs:17-32, :172-181). Both parts may be empty; a plain string
// key or numeric index is simply the empty-paren case.
struct VAML_CORE_EXPORT PropertyIndexerParam
{
    std::string paren;   // the "(...)" paren string (may be empty)
    std::string value;   // the value string (may be empty; the parser trims trailing ws)
};

// A single parsed index expression: the WPF indexer is a comma-separated list of
// IndexerParamInfo units. The common cases (a single Integer or a single quoted
// StringKey) are exposed in a plain form via the convenience accessors; the full
// param list is always retained for fidelity. Integer is signed int64_t so the
// reader/accessor decides from-end semantics.
struct VAML_CORE_EXPORT PropertyIndex
{
    enum class Kind : std::uint8_t { Integer, StringKey };

    Kind       kind = Kind::Integer;
    std::int64_t integer = 0; // meaningful when kind == Integer
    std::string  str;         // meaningful when kind == StringKey
    std::vector<PropertyIndexerParam> params; // the full WPF param list (>= 1 entry)

    // True iff the parsed list is a single Integer param with an empty paren
    // part (the "Items[2]" / "Items[-1]" case).
    [[nodiscard]] bool is_single_integer() const noexcept
    {
        return kind == Kind::Integer && params.size() == 1 && params[0].paren.empty();
    }
};

enum class PropertySegmentKind : std::uint8_t { Property, Index };

// One step of the parsed path. `source` is the exact authored sub-text of this
// step (e.g. ".Street", "/Items[0]", ".Name") for diagnostics/round-trip.
// `drillIn` is the WPF per-level drill-in mode ('.' → Never, '/' → Always,
// first-level default → IfNeeded).
struct VAML_CORE_EXPORT PropertySegment
{
    PropertySegmentKind kind = PropertySegmentKind::Property;
    DrillIn          drillIn = DrillIn::Never;
    std::string      name;   // property name (kind==Property) or string key (kind==Index)
    PropertyIndex    index;  // meaningful when kind == Index
    std::string      source; // exact authored sub-text (".Street", "[0]", ...)
};

// The parsed representation of a path (a route of property/indexer steps, or a
// single Direct step when the path was empty / `.`).
class VAML_CORE_EXPORT PropertyPath
{
public:
    PropertyPath() = default;

    // Parses `text` into a PropertyPath. Throws PropertyPathException on any
    // Class A syntax error (kind + naming fragment). The returned path owns
    // all segment strings (parse never keeps a view into the caller's text).
    [[nodiscard]] static PropertyPath parse(std::string_view text);

    // Noexcept variant: on SUCCESS `out` is the parsed path and true is
    // returned. On a syntax error `out` is left UNTOUCHED, `*err` is set to the
    // kind (never an unset/Empty false-positive), and false is returned.
    // err may be nullptr (caller only cares about success/failure).
    [[nodiscard]] static bool try_parse(std::string_view text, PropertyPath& out,
                                        PropertyPathError* err) noexcept;

    [[nodiscard]] bool valid() const noexcept { return valid_; }
    // True iff the path is a WPF `Direct` self-binding (empty or exactly `.` /
    // `/`, PathParser.cs:83-89) — evaluates to the whole object.
    [[nodiscard]] bool is_direct() const noexcept { return valid_ && isDirect_; }
    [[nodiscard]] std::size_t segment_count() const noexcept { return segments_.size(); }
    [[nodiscard]] const std::vector<PropertySegment>& segments() const noexcept { return segments_; }
    // The first segment only when the path is a non-direct route.
    [[nodiscard]] const PropertySegment* first_segment() const noexcept
    {
        return (valid_ && !isDirect_ && !segments_.empty()) ? &segments_.front() : nullptr;
    }
    [[nodiscard]] const std::string& original() const noexcept { return original_; }

private:
    // Builds the Direct self-binding path (empty / `.` / `/` text). Internal
    // factory so the private members can be set by the parser.
    static void make_direct(PropertyPath& out, std::string_view text) noexcept;

    bool                        valid_ = false;
    bool                        isDirect_ = false;
    std::string                 original_;
    std::vector<PropertySegment> segments_;
};

// --- Evaluator ------------------------------------------------------------
// Outcome of one property-path evaluation. `ok` distinguishes a resolved read
// from an error; when `ok` is false, `error` names the Class B outcome and
// `segmentIndex` is the segment index that failed (0-based; 0 == the first).
// When `ok` is true, `value` holds the resolved value.
struct VAML_CORE_EXPORT PropertyRead
{
    bool              ok = false;
    PropertyPathError error    = PropertyPathError::Empty;
    Value             value    = Value::Null();
    std::size_t       segmentIndex = 0;
};

// One step-reader: reads `segment` against the CURRENT `node` and returns a
// PropertyRead. This is the REAL injectable step-reader seam — the generic
// reflection-lite the static-typed binding engine needs (NO-LAZY). S241/S242/
// S252 wire the live ObservableObject + collection readers here; S240 ships the
// default dependency-object reader (below) so the engine is genuinely usable
// against registered DPs today.
//
// Contract for a well-behaved reader (what the evaluator relies on):
//   - An UNREADABLE node (a segment kind the node's type cannot serve, e.g. an
//     Index into a plain DependencyObject) returns ok=false, error=NotReadable.
//   - An unknown property returns ok=false, error=MissingProperty.
//   - A resolved Value (including Value::Null — null is a REAL value) returns
//     ok=true. The evaluator interprets a null for a NON-final segment as
//     NullIntermediate; a null for the FINAL segment is a valid resolved result.
using PropertyReader = std::function<PropertyRead(const Value& node, const PropertySegment& segment)>;

// Walks the parsed path through the reader, null-safe.
//   - A Direct path (empty / `.` — path.is_direct()) -> ok=true with the ROOT
//     value (self-binding, the WPF `SourceValueInfo(Direct)` result).
//   - segment_count() == 0 (only reachable for an invalid path) -> ok=false,
//     error=Empty.
//   - A reader failure on any segment -> propagated (ok=false) with that
//     segment's error + segmentIndex.
//   - A reader SUCCESS whose value is Value::Null on a NON-final segment ->
//     ok=false, error=NullIntermediate, segmentIndex = that segment. (Stops;
//     the caller owns the null-result policy.)
//   - A reader SUCCESS on the FINAL segment -> ok=true with the value — even
//     if that value is Value::Null (a null final value is a valid read).
// Purely value-domain: no I/O, no locks (ADR-018 clean — it runs on the
// caller's thread and touches only the reader it is handed).
[[nodiscard]] VAML_CORE_EXPORT PropertyRead evaluate_property_path(
    const Value& root, const PropertyPath& path, const PropertyReader& reader);

} // namespace vaml
