#pragma once
// VAML BindingDiagnostics — the diagnostics record + sink for the binding engine.
// S240_1 (dep:S240). Sprint 6. The reader side of the diagnostics contract:
// a BindingDiagnostic value record (the roadmap field set) describing ONE
// binding failure/outcome, and an IBindingDiagnostics observer sink that the
// binding engine emits to at every failure point. The engine CONSUMERS arrive
// with their own steps — S242 (converter/fallback), S243 (one-way), S244
// (two-way), S245 (lifecycle) — and are forward-contracted below (NO-GAP:
// the reason-code vocabulary is defined NOW, its emission is bound to the named
// emitting step, never a silent backfill).
//
// This is REAL permanent core surface (NOT a STUB / NOT MINIMAL REAL — there is
// no deferred backfill; the record, the class taxonomy and the sink are complete
// on delivery) and is headless-testable on the Shouldly + structured-error
// conventions established by S030..S240.
//
// Two deliberately-split responsibility classes (the S240 split, carried
// forward — diagnostics record it at EMIT so the subscriber never re-derives it):
//   - Class A (StaticAuthoring): a STATIC / authoring error. The binding text,
//     converter signature or configuration is wrong; NO data is needed to know
//     it. The binding fails fast (AuthoringError / throw path).
//   - Class B (DefinedRuntimeOutcome): a DEFINED runtime data outcome. The
//     binding is well-authored but the live data produced a null, a missing
//     property or an unreadable node; the engine applies its fallback policy
//     and reports (never throws).
//
// ADR-016 governs: diagnostics are OBSERVATION, never control flow. The
// subscriber receives information about a failure; it never influences whether
// the failure occurred. `emit` is void and the record is passed by const-ref.

#include "vaml/core/PropertyPath.h" // PropertyPath, PropertyPathError, PropertyRead
#include "vaml/core/export.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace vaml {

// --- Class taxonomy --------------------------------------------------------
// The A/B tag stamped at emit. See the header comment for the split contract.
enum class BindingClass : std::uint8_t
{
    StaticAuthoring      = 0, // Class A
    DefinedRuntimeOutcome = 1 // Class B
};

// The binding's data-flow direction. S242 wiring recognizes these modes; the
// record carries them so a diagnostic is self-describing without the sink
// needing binding-bookkeeping of its own.
enum class BindingDirection : std::uint8_t
{
    OneTime = 0, // resolve once at bind
    OneWay  = 1, // S243: source -> target
    TwoWay  = 2  // S244: source <-> target
};

// --- Reason codes ----------------------------------------------------------
// One enumeration per *reason*, never a free string, so subscribers switch on a
// stable symbol. Every arm is documented with the step whose EMISSION it serves.
// Arms tagged "[fwd]" are forward contracts consumed by later S6 tokens; their
// Class A/B mapping is fully exercised by the S240_1 tests NOW, but their
// emission is bound to the named step (NO-GAP, recorded — not a silent backfill).
enum class BindingReasonCode : std::uint8_t
{
    // ---- Class A — StaticAuthoring ---------------------------------------
    EmptyPath               = 0, // empty/invalid path (S240 resolve; degenerate).
    MalformedPath           = 1, // a PropertyPathError (UnterminatedIndexer, etc.)
                                 // (S240 resolve; real, thrown -> mapped).
    ConverterSignatureMismatch = 2, // [fwd] converter's input/output type(s) do
                                 // not line up with source/target (S242_1 Strict).

    // ---- Class B — DefinedRuntimeOutcome ---------------------------------
    NullIntermediate        = 3, // a NON-final path segment read Value::Null (S240
                                 // resolve; real + testable NOW via PropertyRead).
    MissingProperty         = 4, // the path resolved to an unknown property (S240
                                 // resolve; real + testable NOW via PropertyRead).
    NotReadable             = 5, // a node cannot be read by the current reader
                                 // (S240 resolve; real + testable NOW).
    ConverterFailed         = 6, // [fwd] a converter threw / returned failure (S242).
    FallbackApplied         = 7, // [fwd] a Class B outcome fell back to a bound
                                 // fallback value instead of a null (S242).
    SetTargetFailed         = 8, // [fwd] one-way target assignment was rejected (S243).
    WriteBackRejected       = 9, // [fwd] two-way write-back was rejected (S244).
    Detached                = 10 // [fwd] a two-way binding wrote while detached;
                                 // a SILENCED non-error (S245), reported only so
                                 // a subscribed sink can observe the lifecycle.
};

// The tagging authority: maps a reason code to its class. Single source of
// truth for the A/B split so emitters and subscribers never drift. Defined in
// the .cpp (a compact table, no include-bloat in the header).
[[nodiscard]] VAML_CORE_EXPORT BindingClass binding_class_of(BindingReasonCode code) noexcept;

// --- The diagnostic record -------------------------------------------------
// A single binding failure/outcome. Exact roadmap field set; owned std::string
// (diagnostics are infrequent failure values — no intern discipline needed).
// `timestamp` is stamped at creation. Constructed via the factories below, which
// fill it from a PropertyRead / PropertyPathError and thus guarantee the A/B
// tagging and context are assembled consistently at the emit site.
struct VAML_CORE_EXPORT BindingDiagnostic
{
    std::uint64_t   bindingId = 0;              // the binding instance's id
    std::string     sourcePath;                 // the bound source path text
    std::string     targetProp;                 // the target property name
    BindingDirection direction = BindingDirection::OneTime;
    BindingReasonCode reasonCode = BindingReasonCode::EmptyPath;
    std::string     sourceType;                 // source value type name
    std::string     targetType;                 // target property type name
    std::string     converter;                  // converter name, if any involved
    std::string     detail;                     // free-form authoring/runtime detail
    std::chrono::system_clock::time_point timestamp; // stamped at creation

    // Source location of the authoring site (binding text). May be empty.
    std::string     sourceFile;
    std::uint32_t   sourceLine = 0;

    // The A/B class of this diagnostic — derived through binding_class_of().
    [[nodiscard]] BindingClass bindingClass() const noexcept
    {
        return binding_class_of(reasonCode);
    }
};

// --- The sink --------------------------------------------------------------
// Observer: the binding engine emits a BindingDiagnostic to the CURRENT sink at
// every failure point. The subscriber gets information, never control (ADR-016).
class VAML_CORE_EXPORT IBindingDiagnostics
{
public:
    virtual ~IBindingDiagnostics() = default;
    virtual void emit(const BindingDiagnostic& diagnostic) = 0;
};

// NO-OP default: an empty emit. This is what the app gets until it opts in via
// set_binding_diagnostics(); release cost is zero unless subscribed (emission
// happens only at failure points, and the default body is empty).
class VAML_CORE_EXPORT NullBindingDiagnostics final : public IBindingDiagnostics
{
public:
    void emit(const BindingDiagnostic&) override;
};

// --- Accessors -------------------------------------------------------------
// The shared NO-OP default sink. Function-local static, never null.
[[nodiscard]] VAML_CORE_EXPORT IBindingDiagnostics& default_binding_diagnostics() noexcept;

// The current sink (the no-op until the app opts in). Read is a lockless atomic
// pointer load (ADR-018: zero new locks); safe to call from any thread.
[[nodiscard]] VAML_CORE_EXPORT IBindingDiagnostics& binding_diagnostics() noexcept;

// App opt-in: installs `sink` as the current sink and returns the PREVIOUS sink
// (so the caller can restore it). Intended to be called ONCE at startup before
// bindings are created — the swap is the only write to the atomic pointer. Pass
// anything, but do not pass a stack object that dies before bindings finish.
[[nodiscard]] VAML_CORE_EXPORT IBindingDiagnostics& set_binding_diagnostics(IBindingDiagnostics& sink) noexcept;

// --- Factories -------------------------------------------------------------
// Build a BindingDiagnostic from an S240 resolve outcome (a PropertyRead).
// Both the Class B mapping (NullIntermediate / MissingProperty / NotReadable)
// and the degenerate invalid-path mapping (Empty -> EmptyPath, Class A) are
// handled; `timestamp` is stamped now. `sourceFile` / `sourceLine` are the
// authoring site (default empty/0).
[[nodiscard]] VAML_CORE_EXPORT BindingDiagnostic diagnostic_from_property_read(
    std::uint64_t bindingId, std::string targetProp, BindingDirection direction,
    std::string sourceType, std::string targetType, std::string converter,
    const PropertyPath& path, const PropertyRead& read,
    std::string detail = {}, std::string sourceFile = {}, std::uint32_t sourceLine = 0);

// Build a BindingDiagnostic from a Class A parse failure (a PropertyPathError).
// Maps the syntax error to MalformedPath (or EmptyPath for the Empty kind).
[[nodiscard]] VAML_CORE_EXPORT BindingDiagnostic diagnostic_from_path_error(
    std::uint64_t bindingId, std::string sourcePath, std::string targetProp,
    BindingDirection direction, std::string sourceType, std::string targetType,
    PropertyPathError error, std::string detail = {},
    std::string sourceFile = {}, std::uint32_t sourceLine = 0);

} // namespace vaml
