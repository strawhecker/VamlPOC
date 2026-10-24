// S245_1 (dep:S245) — Binding-FAILURE test suite: the harness SUBSCRIBES to the
// sink and asserts the EXACT reasonCode per Class (not just fallback values);
// detach/lifecycle (S245) is a Class-B-tagged, silenced non-error; Diagnostics
// ≠ control-flow (the subscriber gets info, never control). Sprint 6.
//
// This is the formal binding-failure CONSOLIDATION suite (the S040/S042/S044/
// S125/S128/S130/S133/S135/S137 test-only precedent): every failure arm of the
// real binding engine — Class A (StaticAuthoring) EmptyPath/MalformedPath and
// Class B (DefinedRuntimeOutcome) MissingProperty/NullIntermediate/NotReadable/
// ConverterFailed/FallbackApplied/SetTargetFailed/WriteBackRejected/Detached —
// is exercised through the REAL BindingExpression built on the REAL S241 reader,
// S240 evaluator and S240_1 sink, and asserted by an EXACT reasonCode sequence
// recorded by a subscribing RecordingSink (the S245_1 SinkGuard installs it and
// restores the previous sink at scope exit).
//
// TEST-ONLY step: ZERO new product symbols, ZERO MINIMAL REAL introduced or
// resolved. ADR-016 governs everything — the sink receives observation, never
// control-flow. NO-GAP: adds no new forward contracts; S246 (RelayCommand) is
// the next chain link.
//
// Suite map (the three title obligations):
//   A. EXACT reasonCode per Class (not just fallback values):
//        1  Class A malformed path  -> create nullptr + exactly [MalformedPath],
//           StaticAuthoring, NO fallback (Class A never falls back).
//        2  Class A empty path      -> exactly [EmptyPath], StaticAuthoring,
//           nullptr, NO fallback.
//        3  Class A Strict policy   -> emits [MalformedPath] THEN throws
//           BindingPolicyException — the emit is never the control flow, the
//           throw is; reason still Class A.
//        4  Class B MissingProperty (no fallback) -> exactly [MissingProperty],
//           target keeps default, TransferResult::ResolveFailed.
//        5  Class B MissingProperty + fallback -> exactly
//           [MissingProperty, FallbackApplied], target gets fallback,
//           FallbackApplied -> DefinedRuntimeOutcome.
//        6  Class B NullIntermediate (non-final null) -> exactly
//           [NullIntermediate], Class B.
//        7  Class B NotReadable (null source) -> exactly [NotReadable], Class B.
//        8  Class B ConverterFailed (throwing converter) -> exactly
//           [ConverterFailed] (+ [FallbackApplied] when a fallback is bound),
//           Class B.
//        9  Class B live OneWay SetTargetFailed -> exactly [SetTargetFailed]
//           on a live re-push; the caller-driven FIRST push emits NOTHING
//           (S242/S243 parity re-proof).
//       10  Class B live TwoWay WriteBackRejected (failing convertBack) ->
//           exactly [WriteBackRejected], source untouched.
//   B. Detach/lifecycle (S245) is a Class-B-tagged, silenced non-error:
//       11  HEADLINE — destroying the target auto-detaches; a later write
//           attempt emits exactly [Detached] ONCE, binding_class_of(Detached)
//           -> DefinedRuntimeOutcome, target never dereferenced, never throws.
//   C. Diagnostics ≠ control-flow (the subscriber gets info, never control):
//       12  a Class B missing-property failure run with a NO-OP sink vs a
//           RECORDING sink yields the IDENTICAL TransferResult + target value
//           (pure observation); plus a Class A default-Fallback create returns
//           nullptr with the diagnostic still emitted.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyPointer (SHOULD seam)

#include "vaml/core/BindingDiagnostics.h"
#include "vaml/core/BindingExpression.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyMetadata.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using vaml::BindingClass;
using vaml::BindingConversion;
using vaml::BindingExpression;
using vaml::BindingExpressionSpec;
using vaml::BindingMode;
using vaml::BindingPolicy;
using vaml::BindingPolicyException;
using vaml::BindingReasonCode;
using vaml::DependencyObject;
using vaml::IBindingDiagnostics;
using vaml::ObservableObject;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::TransferResult;
using vaml::Value;
using vaml::ValueType;
using vaml::binding_class_of;
using vaml::binding_diagnostics;
using vaml::set_binding_diagnostics;

namespace {

// ---- S245_1-unique names ----
const char* kText    = "S245_1Text";      // source String / target String
const char* kInt     = "S245_1Int";       // Int32 target (wrong-type pushes land here)
const char* kAbsent  = "S245_1Absent";    // absent source name (MissingProperty)
const char* kNullLead = "S245_1NullLead"; // two-segment path lead holding a stored null

PropertyKey key(const std::string& name, ValueType t)
{
    return PropertyKey(name, t, "S245_1Test", PropertyFlags::None);
}

// RAII guard: swaps in the recorder, restores the previous sink at scope exit.
struct SinkGuard
{
    IBindingDiagnostics& previous;
    explicit SinkGuard(IBindingDiagnostics& sink)
        : previous(set_binding_diagnostics(sink)) {}
    ~SinkGuard() { (void)set_binding_diagnostics(previous); }
};

struct RecordingSink final : IBindingDiagnostics
{
    std::vector<BindingReasonCode> reasons;
    void emit(const vaml::BindingDiagnostic& d) override { reasons.push_back(d.reasonCode); }
};

int rc(BindingReasonCode c) { return static_cast<int>(c); }
int tr(TransferResult r) { return static_cast<int>(r); }
int cls(BindingClass c) { return static_cast<int>(c); }

// A OneTime binding over `vm` -> `target` (text DP) with no converter/fallback.
std::unique_ptr<BindingExpression> make_onetime(ObservableObject& vm, DependencyObject& target,
                                                const char* sourcePath = kText)
{
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = sourcePath;
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneTime;
    return BindingExpression::create(spec);
}

// Registers the S245_1 text (String) + int (Int32) DPs on `target` once.
void register_target(DependencyObject& target)
{
    target.register_property(key(kText, ValueType::String),
        PropertyMetadata::WithDefault(Value::String("DEFAULT"), PropertyFlags::None));
    target.register_property(key(kInt, ValueType::Int32),
        PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
}

} // namespace

// ===========================================================================
// 1/2 - CLASS A: malformed + empty path -> exactly [MalformedPath]/[EmptyPath]
// ===========================================================================

TEST_CASE("S245_1 Class A: malformed path -> create nullptr + exactly [MalformedPath], no fallback")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("Hello"));
    DependencyObject target;
    register_target(target);
    RecordingSink sink;
    SinkGuard guard(sink);

    // Malformed: consecutive-dot path fails PARSE (Class A) — never evaluates.
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = "a..b"; // ConsecutiveDot
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneTime;
    spec.fallback = Value::String("FB"); // Class A NEVER falls back

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyPointer(expr.get()).ShouldBeNull()); // no bind
    // EXACT reasonCode sequence.
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::MalformedPath)));
    // Class A: StaticAuthoring (NOT a silent fallback — no [FallbackApplied]).
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::MalformedPath)))
               .ShouldBe(cls(BindingClass::StaticAuthoring)));
    // The fallback was NOT applied (Class A never falls back; target untouched).
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("DEFAULT")));
}

TEST_CASE("S245_1 Class A: empty path -> exactly [EmptyPath], StaticAuthoring, nullptr, no fallback")
{
    ObservableObject vm;
    DependencyObject target;
    register_target(target);
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = ""; // empty -> EmptyPath (Class A)
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneTime;
    spec.fallback = Value::String("FB");

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyPointer(expr.get()).ShouldBeNull());
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::EmptyPath)));
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::EmptyPath)))
               .ShouldBe(cls(BindingClass::StaticAuthoring)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("DEFAULT")));
}

// ===========================================================================
// 3 - CLASS A STRICT: emit THEN throw (emit is never the control flow)
// ===========================================================================

TEST_CASE("S245_1 Class A Strict: emits [MalformedPath] THEN throws BindingPolicyException")
{
    ObservableObject vm;
    DependencyObject target;
    register_target(target);
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = "a[0"; // UnterminatedIndexer (a real Class A parse error)
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneTime;
    spec.policy = BindingPolicy::Strict;

    // The diagnostic is STILL emitted (observation, ADR-016) — and the throw is
    // the control flow, never the emit. Exactly [MalformedPath] on the sink.
    // (Raw try/catch matches the established BindingPolicy_test convention —
    // Strict bind-time promotion never reaches a Shouldly type-seam.)
    bool threw = false;
    try
    {
        (void)BindingExpression::create(spec);
    }
    catch (const BindingPolicyException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(threw).ShouldBe(true));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::MalformedPath)));
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::MalformedPath)))
               .ShouldBe(cls(BindingClass::StaticAuthoring)));
}

// ===========================================================================
// 4/5 - CLASS B: MissingProperty with / without fallback -> exact sequence
// ===========================================================================

TEST_CASE("S245_1 Class B: MissingProperty (no fallback) -> exactly [MissingProperty], ResolveFailed, target default")
{
    ObservableObject vm; // does NOT store kAbsent -> missing property at resolve
    DependencyObject target;
    register_target(target);
    RecordingSink sink;
    SinkGuard guard(sink);

    auto expr = make_onetime(vm, target, kAbsent);
    SHOULD(ShouldlyPointer(expr.get()).ShouldNotBeNull()); // well-authored -> binds

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr(r)).ShouldBe(tr(TransferResult::ResolveFailed)));
    // EXACT sequence: the single Class B resolve reason, no fallback to follow.
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::MissingProperty)));
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::MissingProperty)))
               .ShouldBe(cls(BindingClass::DefinedRuntimeOutcome)));
    // Target keeps its default (nothing to push).
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("DEFAULT")));
}

TEST_CASE("S245_1 Class B: MissingProperty + fallback -> exactly [MissingProperty, FallbackApplied], target gets fallback")
{
    ObservableObject vm;
    DependencyObject target;
    register_target(target);
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kAbsent;
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneTime;
    spec.fallback = Value::String("FB");
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyPointer(expr.get()).ShouldNotBeNull());

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr(r)).ShouldBe(tr(TransferResult::FallbackApplied)));
    // EXACT sequence: the failing reason, THEN the fallback recovery.
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{2}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::MissingProperty)));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[1])).ShouldBe(rc(BindingReasonCode::FallbackApplied)));
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::MissingProperty)))
               .ShouldBe(cls(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::FallbackApplied)))
               .ShouldBe(cls(BindingClass::DefinedRuntimeOutcome)));
    // The fallback VALUE reaches the target (the observable recovery).
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("FB")));
}

// ===========================================================================
// 6 - CLASS B: NullIntermediate (non-final null)
// ===========================================================================

TEST_CASE("S245_1 Class B: NullIntermediate (non-final null) -> exactly [NullIntermediate], Class B")
{
    ObservableObject vm;
    // A stored null on a NON-final segment of a two-segment path: the evaluator
    // null-stops before the second segment -> NullIntermediate (Class B).
    (void)vm.Set(PropertyId::of(kNullLead), Value::Null());
    DependencyObject target;
    register_target(target);
    RecordingSink sink;
    SinkGuard guard(sink);

    const std::string twoHop = std::string(kNullLead) + ".Child";
    auto expr = make_onetime(vm, target, twoHop.c_str());
    SHOULD(ShouldlyPointer(expr.get()).ShouldNotBeNull());

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr(r)).ShouldBe(tr(TransferResult::ResolveFailed)));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::NullIntermediate)));
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::NullIntermediate)))
               .ShouldBe(cls(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("DEFAULT")));
}

// ===========================================================================
// 7 - CLASS B: NotReadable (null source root)
// ===========================================================================

TEST_CASE("S245_1 Class B: NotReadable (null source) -> exactly [NotReadable], Class B")
{
    DependencyObject target;
    register_target(target);
    RecordingSink sink;
    SinkGuard guard(sink);

    // Binding with a NULL source: the evaluator's root is Value::Null(), which
    // the ObservableObject reader cannot read -> NotReadable (Class B).
    BindingExpressionSpec spec;
    spec.source = nullptr;
    spec.sourcePath = kText;
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneTime;
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyPointer(expr.get()).ShouldNotBeNull());

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr(r)).ShouldBe(tr(TransferResult::ResolveFailed)));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::NotReadable)));
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::NotReadable)))
               .ShouldBe(cls(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("DEFAULT")));
}

// ===========================================================================
// 8 - CLASS B: ConverterFailed (throwing converter) + optional fallback
// ===========================================================================

TEST_CASE("S245_1 Class B: ConverterFailed (throwing converter) -> exactly [ConverterFailed], then [FallbackApplied] when bound")
{
    // Arm A: no fallback -> exactly [ConverterFailed], TransferResult::ConverterFailed.
    {
        ObservableObject vm;
        (void)vm.Set(PropertyId::of(kText), Value::String("Hello"));
        DependencyObject target;
        register_target(target);
        RecordingSink sink;
        SinkGuard guard(sink);

        BindingExpressionSpec spec;
        spec.source = &vm;
        spec.sourcePath = kText;
        spec.target = &target;
        spec.targetProp = kText;
        spec.mode = BindingMode::OneTime;
        spec.converter.convert = [](const Value&) -> BindingConversion {
            throw std::runtime_error("boom");
        };
        spec.converterName = "S245_1Throwing";
        auto expr = BindingExpression::create(spec);
        SHOULD(ShouldlyPointer(expr.get()).ShouldNotBeNull());

        TransferResult r = expr->TransferSourceToTarget();
        SHOULD(ShouldlyIntegral(tr(r)).ShouldBe(tr(TransferResult::ConverterFailed)));
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::ConverterFailed)));
        SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::ConverterFailed)))
                   .ShouldBe(cls(BindingClass::DefinedRuntimeOutcome)));
        // Without a fallback the target keeps its default.
        SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("DEFAULT")));
    }
    // Arm B: converter failure + fallback -> exactly [ConverterFailed, FallbackApplied],
    // target gets the fallback value.
    {
        ObservableObject vm;
        (void)vm.Set(PropertyId::of(kText), Value::String("Hello"));
        DependencyObject target;
        register_target(target);
        RecordingSink sink;
        SinkGuard guard(sink);

        BindingExpressionSpec spec;
        spec.source = &vm;
        spec.sourcePath = kText;
        spec.target = &target;
        spec.targetProp = kText;
        spec.mode = BindingMode::OneTime;
        spec.fallback = Value::String("CFB");
        spec.converter.convert = [](const Value&) -> BindingConversion {
            throw std::runtime_error("boom");
        };
        spec.converterName = "S245_1Throwing";
        auto expr = BindingExpression::create(spec);

        TransferResult r = expr->TransferSourceToTarget();
        SHOULD(ShouldlyIntegral(tr(r)).ShouldBe(tr(TransferResult::FallbackApplied)));
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{2}));
        SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::ConverterFailed)));
        SHOULD(ShouldlyIntegral(rc(sink.reasons[1])).ShouldBe(rc(BindingReasonCode::FallbackApplied)));
        SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("CFB")));
    }
}

// ===========================================================================
// 9 - CLASS B: live OneWay SetTargetFailed (first push silent — S242/S243 parity)
// ===========================================================================

TEST_CASE("S245_1 Class B: live OneWay SetTargetFailed -> exactly [SetTargetFailed]; first push silent")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("str"));
    DependencyObject target;
    target.register_property(key(kInt, ValueType::Int32),
        PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;   // String source
    spec.target = &target;
    spec.targetProp = kInt;    // Int32 target => wrong-typed push -> rejection
    spec.mode = BindingMode::OneWay;
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyPointer(expr.get()).ShouldNotBeNull());

    // Caller-driven FIRST push: rejected (TargetRejected) but emits NOTHING
    // (S242/S243 parity re-proof — the arm is gated to the LIVE re-push).
    TransferResult first = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr(first)).ShouldBe(tr(TransferResult::TargetRejected)));
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true));

    // A LIVE one-way re-push (real source change) is rejected -> emits
    // SetTargetFailed EXACTLY once (the exact reasonCode, Class B).
    sink.reasons.clear();
    (void)vm.Set(PropertyId::of(kText), Value::String("live"));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::SetTargetFailed)));
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::SetTargetFailed)))
               .ShouldBe(cls(BindingClass::DefinedRuntimeOutcome)));
    // Target untouched (still the -1 default).
    SHOULD(ShouldlyIntegral(target.Get(PropertyId::of(kInt)).get<std::int32_t>()).ShouldBe(-1));
}

// ===========================================================================
// 10 - CLASS B: live TwoWay WriteBackRejected (failing convertBack)
// ===========================================================================

TEST_CASE("S245_1 Class B: live TwoWay WriteBackRejected -> exactly [WriteBackRejected], source untouched")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("initial"));
    DependencyObject target;
    register_target(target);
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::TwoWay;
    // convert: passthrough; convertBack: always FAILS -> a target edit cannot
    // write back -> WriteBackRejected.
    spec.converter.convert = [](const Value& v) -> BindingConversion {
        return BindingConversion{ true, v, {} };
    };
    spec.converter.convertBack = [](const Value&) -> BindingConversion {
        return BindingConversion{ false, Value::Null(), "convertBack rejected" };
    };
    spec.converterName = "S245_1TwoWay";
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyPointer(expr.get()).ShouldNotBeNull());

    // Initial forward push applies cleanly (no diagnostic — a SUCCESS is not a
    // failure; only the caller-driven push, which emits nothing anyway).
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("initial")));
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true));

    // A target edit (TextBox typing) fires the live TWO-WAY write-back; the
    // failing convertBack REJECTS the write-back -> exactly [WriteBackRejected].
    (void)target.Set(PropertyId::of(kText), Value::String("edited"));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::WriteBackRejected)));
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::WriteBackRejected)))
               .ShouldBe(cls(BindingClass::DefinedRuntimeOutcome)));
    // The rejected value is NOT written to the source (source untouched).
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("initial")));
}

// ===========================================================================
// 11 - HEADLINE: detach/lifecycle (S245) is a Class-B-tagged, silenced non-error
// ===========================================================================

TEST_CASE("S245_1 HEADLINE: destroying the target auto-detaches; write emits [Detached] once, Class B, never throws")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("initial"));
    RecordingSink sink;
    SinkGuard guard(sink);

    // Build the binding inside a scope; destroying the target element triggers
    // the S245 destruction wire -> auto-detach (no dangling write, no throw).
    auto expr = [&]() -> std::unique_ptr<BindingExpression> {
        auto target = std::make_unique<DependencyObject>();
        register_target(*target);
        auto e = make_onetime(vm, *target);
        (void)e->TransferSourceToTarget();
        target.reset(); // element-destroy lifecycle -> detach before reclaim
        return e;
    }();

    // Detached: the binding is inert, its destruction wire was unsubscribed
    // (normal teardown emits nothing).
    SHOULD(ShouldlyIntegral(expr->detached() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true));

    // A later write attempt (the would-be dangling-target write) is SILENCED:
    // emits Detached EXACTLY ONCE, never dereferences the freed target, never
    // throws.
    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr(r)).ShouldBe(tr(TransferResult::TargetRejected)));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::Detached)));
    // Class B: a silenced NON-error (NOT a StaticAuthoring error, NOT conflated
    // with the forward-set / write-back failure arms).
    SHOULD(ShouldlyIntegral(cls(binding_class_of(BindingReasonCode::Detached)))
               .ShouldBe(cls(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyIntegral(rc(BindingReasonCode::Detached))
               .ShouldNotBe(rc(BindingReasonCode::SetTargetFailed)));
    SHOULD(ShouldlyIntegral(rc(BindingReasonCode::Detached))
               .ShouldNotBe(rc(BindingReasonCode::WriteBackRejected)));

    // A SECOND write attempt emits NOTHING more (Detached is once-only).
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
}

// ===========================================================================
// 12 - Diagnostics ≠ control-flow: the subscriber observes, never steers
// ===========================================================================

TEST_CASE("S245_1 diagnostics != control-flow: NO-OP vs RECORDING sink -> IDENTICAL TransferResult + target")
{
    // Phase 1: a Class B missing-property failure with the DEFAULT (no-op) sink.
    // The default sink emits into the void; the transfer outcome must be the
    // SAME as when a recording sink observes it (observation, not control-flow).
    TransferResult noopResult = TransferResult::Applied;
    Value noopTarget = Value::Null();
    {
        ObservableObject vm;
        DependencyObject target;
        register_target(target);
        // NO SinkGuard installed -> the default no-op sink is active.
        auto expr = make_onetime(vm, target, kAbsent);
        noopResult = expr->TransferSourceToTarget();
        noopTarget = target.Get(PropertyId::of(kText));
    }

    // Phase 2: the SAME failure with a RECORDING sink subscribed.
    TransferResult recResult = TransferResult::Applied;
    Value recTarget = Value::Null();
    std::vector<BindingReasonCode> recorded;
    {
        ObservableObject vm;
        DependencyObject target;
        register_target(target);
        RecordingSink sink;
        SinkGuard guard(sink);
        auto expr = make_onetime(vm, target, kAbsent);
        recResult = expr->TransferSourceToTarget();
        recTarget = target.Get(PropertyId::of(kText));
        recorded = sink.reasons;
    }

    // The sink changed NOTHING: identical TransferResult + identical target
    // state. The recording sink only received the (observable) reason.
    SHOULD(ShouldlyIntegral(tr(noopResult)).ShouldBe(tr(recResult)));
    SHOULD(ShouldlyIntegral(tr(noopResult)).ShouldBe(tr(TransferResult::ResolveFailed)));
    SHOULD(ShouldlyIntegral(recorded.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc(recorded[0])).ShouldBe(rc(BindingReasonCode::MissingProperty)));
    SHOULD(ShouldlyString(noopTarget).ShouldBe(recTarget));
    SHOULD(ShouldlyString(recTarget).ShouldBe(Value::String("DEFAULT")));

    // Phase 3: a Class A default-Fallback create returns nullptr (no bind)
    // while the diagnostic is STILL emitted — the emit is observation, never
    // the control flow (the return-null is the control flow, the reason is
    // reported alongside).
    {
        ObservableObject vm;
        DependencyObject target;
        register_target(target);
        RecordingSink sink;
        SinkGuard guard(sink);
        BindingExpressionSpec spec;
        spec.source = &vm;
        spec.sourcePath = "bad[";
        spec.target = &target;
        spec.targetProp = kText;
        spec.mode = BindingMode::OneTime;
        auto expr = BindingExpression::create(spec);
        SHOULD(ShouldlyPointer(expr.get()).ShouldBeNull());
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(rc(sink.reasons[0])).ShouldBe(rc(BindingReasonCode::MalformedPath)));
    }
}
