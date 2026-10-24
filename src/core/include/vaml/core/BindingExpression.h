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
// S242 OWNS: the mode type, the expression config, the initial source->target
// transfer (the OneTime resolve + the first push OneWay/TwoWay build on), the
// converter-Convert wire, the Class B fallback application, and the emission of
// ConverterFailed + FallbackApplied (plus the S240 resolve arms). S242
// deliberately does NOT emit SetTargetFailed and does NOT implement live
// subscriptions or Strict policy.
//
// S243 OWNS (this step, dep:S242): the LIVE ONE-WAY source->target
// subscription — a OneWay expression arms a per-property subscription onto the
// source ObservableObject (keyed on the FIRST PATH SEGMENT's property name) via
// S241 add_property_changed; on a source change the handler re-runs the exact
// S242 pipeline (TransferSourceToTarget: re-resolve -> re-convert ->
// re-apply-fallback -> re-push to the target DP). S243 also owns the S240_1
// SetTargetFailed FORWARD ARM (tagged `[fwd->S243]` in BindingDiagnostics.h):
// a one-way transfer whose target Set is rejected emits
// BindingReasonCode::SetTargetFailed (Class B, observation never control-flow
// per ADR-016). The subscription is RAII-bound to the expression — the dtor
// removes it and the move ctor/assign re-arm it onto the new `this` so a moved
// one-way binding stays live and a moved-from never fires a dangling this. The
// caller-driven first push (TransferSourceToTarget) is UNCHANGED — `create`
// does not change that shape, and the initial push does not emit (the emission
// is gated to the live one-way re-push, so S242's OneTime no-emission tests
// stay green). DP invalidation coalescing is realized from the real-change
// discipline already established (S241 source Set fires only on a real change;
// S035 target Set returns NO_CHANGE on an equal push) — a burst of source
// writes coalesces to the distinct effective writes only.
//
// NO-GAP — all recorded forward, never a silent backfill: TwoWay (S244)
// consumes this source-side subscription layer as its shared forward wire and
// adds the target->source write-back separately; {Binding} markup (S252)
// resolves onto this expression; multi-hop path re-observation is a
// S252/S244+ concern (S243 subscribes to the leading segment, documented not
// silent). The caller lifetime contract (unchanged from S242, documented): a
// binding must not outlive its source ObservableObject (raw
// ObservableObject*).
//
// S244 OWNS (this step, dep:S243): the TARGET->SOURCE write-back direction of
// a TwoWay binding. The forward (source->target) wire is the EXACT S243 layer,
// now armed for BOTH OneWay and TwoWay (the shared wire, no duplication); the
// reverse direction is a SECOND, additive subscription on the TARGET
// DependencyObject (S041 add_property_changed keyed on targetProp_) — the
// "target changed" signal, which models a TextBox edit landing on the target
// DP. On a real target change, `transfer_write_back()` reads the target's
// current value, runs the `convertBack` reverse-converter wire (S242 declared
// convertBack an empty seam; S244 RESOLVES it — NO-GAP, in place), and writes
// it back to the source via the FIRST PATH SEGMENT's key. A rejection (no
// source, no leading key, or a failed convertBack) emits
// BindingReasonCode::WriteBackRejected (the S240_1 `[fwd->S244]` arm, Class B,
// observation never control-flow per ADR-016 — never a throw). Loop-safety is
// TWO-fold, matching the authoritative WPF primitive (BindingExpressionBase:
// NeedsUpdate + IsInTransfer):
//   (a) the real-change discipline ALREADY established (S241 source Set fires
//       only on a real change; S035 target Set returns NO_CHANGE on an equal
//       push) — a write-back to an equal source and a forward push to an equal
//       target both terminate, and
//   (b) the `transferring_` IsInTransfer-equivalent guard — the WHOLE forward
//       source->target transfer (first push AND live re-push) runs with
//       `transferring_` set, and while it is set a target change is NOT written
//       back to the source. This closes the non-injective-converter round-trip
//       that (a) alone cannot (a forward push that lands on the target with a
//       value different from the user's typed value would otherwise write back).
//       A source->target->write-back->source cycle is therefore BOUNDED
//       (proven by a bounded-cycle test).
// The write-back subscription is RAII-bound (dtor unsubscribes both; move re-arms
// both onto the new `this`). Emission is gated to the LIVE TwoWay write-back
// path exactly as S243 gated SetTargetFailed to the live one-way path: the
// caller-driven first push and the OneWay/OneTime paths emit NOTHING for the
// write-back arm. `WriteBackRejected != SetTargetFailed` (rejected write-back
// vs rejected forward Set are distinct, tested). Caller lifetime contract
// UNCHANGED (a binding must not outlive its source ObservableObject or target
// DependencyObject).
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
// was refused / wrong-typed.
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
    // repeatedly (OneTime re-resolves on demand; the live one-way re-push is
    // S243). Never throws on a Class B outcome or a target rejection (those
    // map to TransferResult + diagnostics). The OneTime path does NOT emit
    // SetTargetFailed (S243's arm is gated to the live one-way re-push).
    [[nodiscard]] TransferResult TransferSourceToTarget();

    // --- Live one-way subscription introspection (S243) --------------------
    // True when this expression currently holds a live one-way subscription on
    // its source (OneWay mode + a valid source). OneTime never subscribes.
    // S244 broadens the arm to TwoWay too (TwoWay reuses this forward wire).
    [[nodiscard]] bool live_subscribed() const noexcept { return subscribed_; }

    // --- Live write-back introspection (S244) ------------------------------
    // True when this TwoWay expression currently holds a live target->source
    // write-back subscription (TwoWay mode + a valid target + a keyable path).
    // OneWay/OneTime never write back.
    [[nodiscard]] bool live_writeback() const noexcept { return writeback_; }

    // --- S245 detach lifecycle (detach on element destroy) -----------------
    // True once the expression is DETACHED (see Detach()). A detached binding
    // is inert: it writes NOTHING to the target or the source (guards in
    // TransferSourceToTarget / transfer_write_back), has NO live subscriptions
    // (all wires unsubscribed), and a subsequent source/target change emits the
    // S240_1 `BindingReasonCode::Detached` diagnostic (Class B, a SILENCED
    // non-error, ADR-016) ONCE so a subscribed sink can observe the lifecycle —
    // it NEVER dereferences a destroyed target.
    [[nodiscard]] bool detached() const noexcept { return detached_; }

    // Detaches the binding: unsubscribes the forward (S243/S244 shared) + the
    // write-back (S244) wires, marks the expression detached. Idempotent, never
    // throws, emits NOTHING (normal teardown — the lifecycle emission is gated
    // to an ATTEMPTED WRITE on an already-detached binding, not to this call).
    void Detach();

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
    // TransferResult. A wrong-typed / refused Set -> TargetRejected.
    [[nodiscard]] TransferResult apply_to_target(const Value& value);

    // S245: emits BindingReasonCode::Detached (the S240_1 forward arm, Class B,
    // a SILENCED non-error) once. Gated to an attempted WRITE on a detached
    // binding (never at Detach(), never at create).
    void emit_detached();

    // S245: the destruction-lifecycle handler resolved from a target's
    // DependencyObject::add_destroyed token. Firing means the target ELEMENT is
    // being destroyed — the binding must detach BEFORE the target's memory is
    // reclaimed so no later write dereferences a freed target.
    void on_target_destroyed();

    // S245: removes the target->destroyed subscription (no-op when not armed);
    // re-arms it onto `this` after a move. Never throws.
    void unsubscribe_destroyed();

    // S245: arms the target->destroyed subscription (detach on element
    // destroy). Idempotent; no-op when already armed, when there is no target,
    // or when detached. Never throws.
    void arm_destroyed();

    // The S243 live-subscription arm (OneWay mode). Arms a per-property
    // subscription onto source_ keyed on the FIRST PATH SEGMENT's property
    // name (S241 add_property_changed); on a real source change the handler
    // re-runs TransferSourceToTarget() and — only for the OneWay path — emits
    // a SetTargetFailed diagnostic when that transfer is TargetRejected (the
    // S240_1 forward arm, bound here). Idempotent: arming when already
    // subscribed is a no-op. Never throws.
    void arm_subscription();
    // Removes the live subscription (no-op when not subscribed). Idempotent.
    void unsubscribe();
    // The shared S243 re-push body used by the live handler: runs
    // TransferSourceToTarget() and emits SetTargetFailed (once, Class B) when
    // the transfer was rejected — the emission is S243's, gated to the live
    // one-way path so S242's OneTime no-emission tests stay green.
    void transfer_one_way();

    // --- S244 two-way write-back layer -------------------------------------
    // Arms a per-property subscription on the TARGET DependencyObject
    // (S041 add_property_changed keyed on targetProp_) — the "target changed"
    // signal (a TextBox edit landing on the target DP). On a real target
    // change the handler runs transfer_write_back(). Idempotent; no-op when
    // the mode is not TwoWay, when already armed, when there is no target or
    // no keyable path. Never throws.
    void arm_writeback();
    // Removes the write-back subscription (no-op when not armed). Idempotent.
    void unsubscribe_writeback();
    // The target->source write-back body (see the class header comment): reads
    // the target value, runs convertBack (S244 wire), writes back to the
    // source. Emits WriteBackRejected on a rejection (Class B, never throws).
    void transfer_write_back();
    // Emits BindingReasonCode::WriteBackRejected (the S240_1 forward arm,
    // Class B) with the given detail. Gated to the live TwoWay write-back
    // path — the caller-driven first push and OneWay/OneTime never reach here.
    void emit_write_back_rejected(const std::string& detail);

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

    // S243 live-subscription state. `liveToken_` is valid only while
    // `subscribed_`; a moved-from / destroyed expression never fires a dangling
    // `this` (move re-arms, dtor unsubscribes). Default-constructed value for a
    // never-subscribed expression; token is meaningless then.
    bool subscribed_ = false;
    ObservableObject::PropertyChangedToken liveToken_ = 0;

    // S244 two-way write-back state. `targetToken_` is valid only while
    // `writeback_`; a moved-from / destroyed expression never fires a dangling
    // `this` (move re-arms, dtor unsubscribes). Default token meaningless for a
    // never-armed expression.
    bool writeback_ = false;
    DependencyObject::PropertyChangedToken targetToken_ = 0;

    // S244 IsInTransfer-equivalent guard (the authoritative WPF primitive):
    // true while a forward source->target transfer is in flight (set in
    // TransferSourceToTarget, cleared at scope exit). While true, a target
    // change is suppressed from writing back to the source. This closes the
    // non-injective-converter round-trip gap that the real-change discipline
    // alone cannot (a forward push landing on the target with a value different
    // from what the user typed). Not copied on move (moved-from is inert).
    bool transferring_ = false;

    // S245 lifecycle state. `detached_` marks an inert binding (see Detach()).
    // `destroyedToken_` is valid only while `destroyedArmed_`; a moved-from /
    // destroyed expression never fires a dangling `this` (move re-arms, dtor
    // unsubscribes) and never holds a callback into a dead target.
    bool detached_ = false;
    bool destroyedArmed_ = false;
    DependencyObject::DestroyedToken destroyedToken_ = 0;
    bool detachedEmitted_ = false; // Detached emitted at most once
};

} // namespace vaml
