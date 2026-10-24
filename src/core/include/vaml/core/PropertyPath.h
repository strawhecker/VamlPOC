#pragma once
// VAML PropertyPath — the parsed dot-path binding route + its evaluator.
// S240 (dep:S226). Sprint 6 first token. Foundation for the binding engine.
//
// This is REAL permanent core surface (NOT a STUB / NOT MINIMAL REAL — there
// is no deferred backfill; the type is complete on delivery) and is headless-
// testable on the `vaml::Value` + structured-error + Shouldly conventions
// established by S030..S208.
//
// Two distinct, deliberately-split responsibility classes (this split is the
// S240_1/BindingDiagnostic precondition — recorded, NOT silent):
//   - Class A (syntax / authoring): PropertyPathError's Empty, TrailingDot,
//     ConsecutiveDot, UnterminatedIndexer, EmptyIndexer, MalformedIndex,
//     UnclosedQuote — thrown by parse() as PropertyPathException. Static:
//     the path TEXT is wrong and no data is needed to know it.
//   - Class B (evaluation / runtime data outcome): NullIntermediate,
//     MissingProperty, NotReadable — produced by the EVALUATOR (never by
//     parse()). They depend on the live data, so they are reported back as
//     PropertyRead outcomes, not thrown diagnostics.
//
// Grammar (hand-rolled, WPF-faithful): a path is a sequence of `.name` /
// `[index]` route steps. The first step may be bare (`Address`, `[0]`,
// `Address[0].Name`). A name is a non-empty run excluding `.` `[` `]` and
// whitespace. An index is either an optional `-` + digits (Integer) or a
// single/double-quoted string (StringKey). Whitespace around dots and inside
// brackets is skipped (authoring tolerance), but whitespace WITHIN a name is
// a syntax boundary.
//
// The parser is hand-rolled (no std::regex, no parser-generator dependency):
// a dot-path grammar is small and deterministic, and hand-rolling keeps this
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
    Empty             = 0,  // empty / whitespace-only path
    TrailingDot       = 1,  // "Address."        (ends on a dot)
    ConsecutiveDot    = 2,  // "Address..Street" (a dot adjacent to a dot)
    UnterminatedIndexer = 3,// "Items[0"        (bracket never closed)
    EmptyIndexer      = 4,  // "Items[]"        (no index inside brackets)
    MalformedIndex    = 5,  // "Items[abc]" / "Items[1.5]" / "Items[-]" (bad index)
    UnclosedQuote     = 6,  // "Items[\"abc]    (quote never closed)

    // Class B — evaluation (runtime data outcome, never thrown by parse()).
    NullIntermediate  = 7,  // a NON-final segment yielded Value::Null
    MissingProperty   = 8,  // the read resolved to an unknown property
    NotReadable       = 9,  // the node cannot be read by this reader (e.g. an
                            // index into a non-indexable DependencyObject)
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

// A single parsed index expression: either an Integer (optionally negative) or
// a quoted StringKey. Integer is signed int64_t so the reader/accessor decides
// from-end semantics; the parser only records the literal sign.
struct VAML_CORE_EXPORT PropertyIndex
{
    enum class Kind : std::uint8_t { Integer, StringKey };

    Kind       kind = Kind::Integer;
    std::int64_t integer = 0; // meaningful when kind == Integer
    std::string  str;         // meaningful when kind == StringKey
};

enum class PropertySegmentKind : std::uint8_t { Property, Index };

// One step of the parsed path. `source` is the exact authored sub-text of this
// step (e.g. ".Street", "[0]", ".Name") for diagnostics/round-trip.
struct VAML_CORE_EXPORT PropertySegment
{
    PropertySegmentKind kind = PropertySegmentKind::Property;
    std::string      name;   // property name (kind==Property) or string key (kind==Index)
    PropertyIndex    index;  // meaningful when kind == Index
    std::string      source; // exact authored sub-text (".Street", "[0]", ...)
};

// The parsed representation of a dot-path.
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
    [[nodiscard]] std::size_t segment_count() const noexcept { return segments_.size(); }
    [[nodiscard]] const std::vector<PropertySegment>& segments() const noexcept { return segments_; }
    [[nodiscard]] const std::string& original() const noexcept { return original_; }

private:
    bool                        valid_ = false;
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
//   - segment_count() == 0 -> ok=false, error=Empty.
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

// The default reader: when `node` is Value::UntypedPtr to a DependencyObject,
// a Property segment reads obj->Get(PropertyId(name)) — mapping the
// unregistered case to MissingProperty (never a thrown raw PropertyNotFound),
// and an Index segment on a plain DependencyObject -> NotReadable (a
// DependencyObject is not indexable; collection-indexable objects get their own
// reader when collections land — NO-GAP, recorded). Any OTHER node/segment
// combination -> NotReadable. Makes S240 genuinely usable today against
// registered DPs on the S036/S039 store.
[[nodiscard]] VAML_CORE_EXPORT PropertyReader dependency_object_reader();

} // namespace vaml
