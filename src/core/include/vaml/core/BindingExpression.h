#pragma once
// VAML BindingExpression — the binding expression: source/target, mode, the
// converter, the fallback value, and the initial/first source->target transfer.
// S242 (dep:S241) . Sprint 6. Built on:
//   - S240  PropertyPath (Class A parse / Class B evaluate) + the injectable
//           PropertyReader seam;
//   - S241  ObservableObject (the VM source axis) + observable_object_reader();
//   - S240_1 BindingDiagnostics + IBindingDiagnostics (the record + sink whose
//           forward-tagged arms — ConverterFailed, FallbackApplied — are emitted
//           HERE).
//
// This is REAL permanent core surface (NOT a STUB / NOT MINIMAL REAL — there is
// no deferred backfill; the expression, mode, converter and fallback pipeline
// are complete and headless-testable on delivery).
//
// S242 OWNS (this step): the mode type, the expression config, the initial
// source->target transfer (the OneTime resolve + the first push OneWay/TwoWay
// build on), the converter-Convert wire, the Class B fallback application, and
// the emission of ConverterFailed + FallbackApplied (plus the S240 resolve
// arms). S242 deliberately does NOT emit SetTargetFailed (that is S243's arm,
// tagged in BindingDiagnostics.h) and does NOT implement live subscriptions
// (S243 one-way, S244 two-way) or Strict policy (S242_1). NO-GAP — all recorded
// forward, never a silent backfill.
//
// The two deliberately-split responsibility classes (S240 split, carried):
//   - Class A (StaticAuthoring): the binding TEXT is wrong. `create` fails fast:
//     it emits a MalformedPath/EmptyPath diagnostic and returns nullptr — it
//     does NOT throw and does NOT bind (the S242 Fallback-policy default: no
//     Class A ever falls back — fallback is Class B-only). S242_1 (Strict)
//     upgrades this to a bind-time throw.
//   - Class B (DefinedRuntimeOutcome): the binding is well-authored but the live
//     data produced a null / missing property / unreadable node, or the converter
//     failed. The engine emits the failure reason and APPLIES the bound fallback
//     value when one is present (ADR-016: diagnostics = observation, never
//     control flow; Class B never throws).
//
// ADR-018: ZERO new locks. A binding is used on a single thread (its target's
// affinity thread for Set, which S037 marshals wrong-thread writes to QUEUED —
// mapped to TargetRejected at S242; live cross-thread marshalling is
// S326/S211 territory, recorded not silent). bindingId is a monotonic
// function-local counter (ADR-018: no shared mutable state).

#include "vaml/core/BindingDiagnostics.h" // BindingDirection, IBindingDiagnostics, factories
#include "vaml/core/DependencyObject.h"   // DependencyObject (the target)
#include "vaml/core/ObservableObject.h"   // ObservableObject (the source) + its reader
#include "vaml/core/PropertyPath.h"       // PropertyPath, PropertyRead, evaluate_property_path
#include "vaml/core/Value.h"              // Value

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace vaml {

// --- Mode ------------------------------------------------------------------
// The binding's data-flow mode (WPF-faithful). OneTime resolves the source ONCE
// at the first transfer; OneWay (S243) and TwoWay (S244) build their live
// subscription onto the same first push this step performs. Values are stable
// uint8_t ordinals.
enum class BindingMode : std::uint8_t
{
    OneTime = 0,
    OneWay  = 1,
    TwoWay  = 2,
};

// --- Policy ----------------------------------------------------------------
// The Class A (bind-time) failure policy per ADR-016. `Fallback` (default) is
// the S242-tested behavior: a Class A path error is emitted as a diagnostic and
// create() returns nullptr (no bind, no throw — Class A never falls back).
// `Strict` (S242_1) PROMOTES that same Class A path error to a BIND-TIME THROW:
// create() emits the diagnostic (observation, ADR-016 — it is never the control
// flow, the throw is) and then throws BindingPolicyException. Class B outcomes
// (NullIntermediate / MissingProperty / NotReadable / ConverterFailed) NEVER
// throw under either policy — they stay TransferResult + diagnostics (fallback
// applies where present).
enum class BindingPolicy : std::uint8_t
{
    Fallback = 0, // default; Class A -> emit + return nullptr (no bind, no throw)
    Strict   = 1, // Class A -> emit + THROW BindingPolicyException (bind-time)
};

// The Strict-policy Class A throw. Carries kind() (the PropertyPathError) and
// path() and names the authored path in what() — the structure-error convention
// established by S082/S091/S094 (PropertyPathException). Strict means: no bind,
// the author is told loudly at bind time instead of silently getting nullptr.
class VAML_CORE_EXPORT BindingPolicyException : public std::logic_error
{
public:
    BindingPolicyException(PropertyPathError kind, std::string path);

    [[nodiscard]] PropertyPathError kind() const noexcept { return kind_; }
    [[nodiscard]] const std::string& path() const noexcept { return path_; }

private:
    PropertyPathError kind_;
    std::string       path_;
};

// Maps a BindingMode to the diagnostic-carrying BindingDirection (S240_1).
[[nodiscard]] VAML_CORE_EXPORT BindingDirection to_binding_direction(BindingMode mode) noexcept;

// --- Converter -------------------------------------------------------------
// One converter invocation. `ok=false` means the converter failed (threw /
// returned failure) — the engine emits ConverterFailed and consults the
// fallback policy. `detail` is free-form (carried into the diagnostic).
struct VAML_CORE_EXPORT BindingConversion
{
    bool  ok = false;
    Value value = Value::Null();
    std::string detail; // converter failure detail (emitted as diagnostic detail)
};

// The converter's Convert signature: takes the resolved source Value, returns
// a BindingConversion. A null std::function means "no converter" (passthrough).
using BindingConverterFn = std::function<BindingConversion(const Value&)>;

// The converter entity. `convert` is the S242 wire (source -> target);
// `convertBack` is the S244 two-way wire (target -> source) — deliberately an
// empty/no-op seam at S242 and RESOLVED by S244 (NO-GAP, recorded, not silent).
struct VAML_CORE_EXPORT BindingConverter
{
    BindingConverterFn convert;
    BindingConverterFn convertBack; // S244 two-way wire (empty/no-op at S242)
};

// --- Transfer outcome ------------------------------------------------------
// The S242 result of one source->target transfer. `Applied`/`NoChange` come from
// the target Set; `ResolveFailed` is a Class B source resolve with NO fallback
// (or a defensive degenerate path); `ConverterFailed` is a failed converter with
// NO fallback; `FallbackApplied` means a Class B resolve / converter failure was
// recovered by the bound fallback value; `TargetRejected` means the target Set
// was refused / wrong-typed (no SetTargetFailed emission — that arm is S243's).
enum class TransferResult
{
    Applied,
    NoChange,
    ResolveFailed,
    ConverterFailed,
    TargetRejected,
    FallbackApplied,
};

// --- Config ----------------------------------------------------------------
// Everything a binding needs at construction. `source` is the ObservableObject
// VM (may be null -> NotReadable at transfer, a Class B outcome); `sourcePath`
// is the dot-path into it; `target` is the DependencyObject the value lands on;
// `targetProp` is its registered property name; `mode` selects data-flow;
// `converter` (optional) transforms the resolved value; `fallback` (optional)
// is the Class B fallback value; `policy` selects Class A bind-time behavior
// (Fallback default: emit + nullptr; Strict: emit + throw); sourceType/
// targetType feed diagnostics; a `converterName` is carried into
// ConverterFailed/FallbackApplied diagnostics.
struct VAML_CORE_EXPORT BindingExpressionSpec
{
    ObservableObject* source = nullptr;
    std::string sourcePath;
    DependencyObject* target = nullptr;
    std::string targetProp;
    BindingMode mode = BindingMode::OneTime;
    BindingPolicy policy = BindingPolicy::Fallback;
    BindingConverter converter;
    std::string converterName;
    std::optional<Value> fallback;
    std::string sourceType; // diagnostic metadata (default empty)
    std::string targetType; // diagnostic metadata (default empty)
};

// --- The expression --------------------------------------------------------
// Move-only RAII unit owning one binding's config + parsed path + first-transfer
// pipeline. `create` gateway: a Class A path error emits a diagnostic and
// returns nullptr (no bind); otherwise a live expression is returned whose
// transfer pipeline runs the S240 evaluator over the S241 ObservableObject
// reader, applies the converter, applies the Class B fallback, and pushes to the
// target DP. Live subscription (S243 one-way / S244 two-way) is a forward step —
// OneTime semantics are fully realized here; OneWay/TwoWay carry their mode
// (tested) and reuse this same first push.
class VAML_CORE_EXPORT BindingExpression
{
public:
    BindingExpression(const BindingExpression&) = delete;
    BindingExpression& operator=(const BindingExpression&) = delete;
    BindingExpression(BindingExpression&&) noexcept;
    BindingExpression& operator=(BindingExpression&&) noexcept;
    ~BindingExpression();

    // Parses spec.sourcePath (Class A gate). On success returns a NEW
    // expression (bindingId already consumed); on a Class A parse failure emits
    // MalformedPath/EmptyPath (observation, ADR-016) and then, per spec.policy:
    //   - Fallback: returns nullptr — NO bind, NO fallback (Class A never
    //     falls back; the S242 behavior).
    //   - Strict: THROWS BindingPolicyException (bind-time throw) — the
    //     S242_1 promotion.
    // Class B outcomes are never reached here (create only parses; B is the
    // transfer's concern), so create never throws for Class B under any policy.
    [[nodiscard]] static std::unique_ptr<BindingExpression> create(const BindingExpressionSpec& spec);

    // Runs the S242 first-transfer pipeline (source resolve -> converter ->
    // fallback -> target Set) and returns the TransferResult. Safe to call
    // repeatedly (OneTime re-resolves on demand; live re-push is S243/S244).
    // Never throws on a Class B outcome or a target rejection (those map to
    // TransferResult + diagnostics).
    [[nodiscard]] TransferResult TransferSourceToTarget();

    // --- Introspection -----------------------------------------------------
    [[nodiscard]] std::uint64_t binding_id() const noexcept { return bindingId_; }
    [[nodiscard]] BindingMode mode() const noexcept { return mode_; }
    [[nodiscard]] BindingPolicy policy() const noexcept { return policy_; }
    [[nodiscard]] ObservableObject* source() const noexcept { return source_; }
    [[nodiscard]] DependencyObject* target() const noexcept { return target_; }
    [[nodiscard]] const std::string& source_path() const noexcept { return sourcePath_; }
    [[nodiscard]] const std::string& target_prop() const noexcept { return targetProp_; }

private:
    BindingExpression() = default;

    // Applies the bound fallback to the target (emits FallbackApplied). Returns
    // true when the fallback path was taken. Only called for a Class B resolve
    // outcome or a converter failure.
    bool apply_fallback();
    // Pushes `value` to the target DP, mapping the Set outcome to a
    // TransferResult. A wrong-typed / refused Set -> TargetRejected (the
    // SetTargetFailed emission is S243's arm, NOT emitted here).
    [[nodiscard]] TransferResult apply_to_target(const Value& value);

    std::uint64_t bindingId_ = 0;
    BindingMode mode_ = BindingMode::OneTime;
    BindingPolicy policy_ = BindingPolicy::Fallback;
    ObservableObject* source_ = nullptr;
    DependencyObject* target_ = nullptr;
    std::string sourcePath_;
    std::string targetProp_;
    BindingConverter converter_;
    std::string converterName_;
    std::optional<Value> fallback_;
    std::string sourceType_;
    std::string targetType_;
    PropertyPath path_;
};

} // namespace vaml
