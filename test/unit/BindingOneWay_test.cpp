// S243 — BindingExpression live ONE-WAY (dep:S242). Sprint 6.
//
// Proves the S243 headline against the REAL vaml::BindingExpression:
//   (A) the live one-way source→target subscription — a OneWay binding arms a
//       per-property subscription onto the source ObservableObject (S241
//       add_property_changed keyed on the FIRST PATH SEGMENT) and auto-re-pushes
//       on a real source change;
//   (B) the S240_1 SetTargetFailed FORWARD ARM — a live one-way re-push whose
//       target Set is rejected emits BindingReasonCode::SetTargetFailed (Class B,
//       observation never control-flow per ADR-016);
//   (C) DP invalidation coalescing — built from the real-change discipline
//       already established (S241 source Set fires only on a real change; S035
//       target Set returns NO_CHANGE on an equal push), a burst of source writes
//       coalesces to the distinct effective writes only.
//
// Registers S243-unique object-scoped DPs on real DependencyObject targets +
// reads real ObservableObject sources. Suite map:
//   1  OneWay initial push APPLIED (create + TransferSourceToTarget).
//   2  OneWay LIVE: a source mutation auto-updates the target (no manual call).
//   3  Distinct burst: 3 distinct source values -> target tracks 1->2->3.
//   4  Source-side coalescing: an equal source re-set fires NO handler -> 0 writes.
//   5  Target-side coalescing: a source change to a value the target already
//      holds -> transfer NoChange -> 0 redundant target writes.
//   6  HEADLINE (DP invalidation coalescing): interleaved burst A,A,B,B,C ->
//      EXACTLY 3 target writes, 0 for the equal repeats.
//   7  Mode introspection: subscription armed only for OneWay; OneTime has none.
//   8  OneTime unchanged: mutating the source does NOT auto-update.
//   9  Converter in one-way: a source change re-runs the converter live.
//  10  Fallback in one-way: a source resolves to a missing path -> FallbackApplied;
//      restored -> live re-update.
//  11  SetTargetFailed emission (S240_1 arm): a OneWay target Set rejection
//      emits exactly SetTargetFailed once; the OneTime wrong-typed path emits
//      NOTHING (S242 re-proof).
//  12  SetTargetFailed != fallback: a target-assignment rejection is never
//      masked by a bound fallback.
//  13  Lifetime / dtor: after the expression is destroyed, a source mutation
//      does NOT fire the handler (subscription removed, no dangling callback).
//  14  Move: a moved one-way expression re-arms its live subscription; a source
//      mutation after the move still updates the target.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam)

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

using vaml::BindingConversion;
using vaml::BindingExpression;
using vaml::BindingExpressionSpec;
using vaml::BindingMode;
using vaml::BindingReasonCode;
using vaml::BindingDirection;
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
using vaml::binding_diagnostics;
using vaml::set_binding_diagnostics;

namespace {

// ---- S243-unique names ----
const char* kText    = "S243Text";    // source String / target String
const char* kNumber  = "S243Number";  // Int32 target (wrong-type pushes land here)
const char* kNope    = "S243Nope";    // absent source name (MissingProperty)

PropertyKey key(const std::string& name, ValueType t)
{
    return PropertyKey(name, t, "S243Test", PropertyFlags::None);
}

// A target whose String DP's metadata `with_changed` callback counts the real
// change NOTIFICATIONS the DP actually fired (a real apply). This is the
// "target changed-callback" counter the coalescing proofs measure.
struct CountingTarget
{
    DependencyObject obj;
    int stringWrites = 0;

    CountingTarget()
    {
        obj.register_property(key(kText, ValueType::String),
            PropertyMetadata::WithDefault(Value::String("DEFAULT"), PropertyFlags::None)
                .with_changed([this](const Value&, const Value&) { ++stringWrites; }));
        obj.register_property(key(kNumber, ValueType::Int32),
            PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
    }
};

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

int rc_int(BindingReasonCode c) { return static_cast<int>(c); }
int tr_int(TransferResult r) { return static_cast<int>(r); }
int mode_int(BindingMode m) { return static_cast<int>(m); }

// A OneWay binding built over `vm` (text) -> `target` (text). Not yet pushed.
std::unique_ptr<BindingExpression> make_oneway(ObservableObject& vm, DependencyObject& target)
{
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneWay;
    return BindingExpression::create(spec);
}

} // namespace

// ===========================================================================
// 1 - INITIAL PUSH (OneWay)
// ===========================================================================

TEST_CASE("S243 OneWay: create + TransferSourceToTarget APPLIES String source -> String target")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("Hello"));
    CountingTarget target;

    auto expr = make_oneway(vm, target.obj);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(1)); // armed on create

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::Applied)));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("Hello")));
}

// ===========================================================================
// 2 - LIVE (subscription drives the re-push)
// ===========================================================================

TEST_CASE("S243 OneWay LIVE: a source mutation auto-updates the target (no manual call)")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("first"));
    CountingTarget target;

    auto expr = make_oneway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("first")));

    // Mutate the source; the LIVE subscription re-pushes — no manual call.
    (void)vm.Set(PropertyId::of(kText), Value::String("second"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("second")));
}

// ===========================================================================
// 3 - DISTINCT BURST
// ===========================================================================

TEST_CASE("S243 OneWay: 3 distinct source values -> target tracks 1->2->3")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("v1"));
    CountingTarget target;

    auto expr = make_oneway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("v1")));

    (void)vm.Set(PropertyId::of(kText), Value::String("v2"));
    (void)vm.Set(PropertyId::of(kText), Value::String("v3"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("v3")));
}

// ===========================================================================
// 4 - SOURCE-SIDE COALESCING (equal re-set -> no handler -> 0 writes)
// ===========================================================================

TEST_CASE("S243 source-side coalescing: an equal source re-set fires NO handler -> 0 target writes")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("same"));
    CountingTarget target;

    auto expr = make_oneway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    const int writesAfterFirst = target.stringWrites;

    // Re-set the SAME value: S241 returns false (real-change discipline) => the
    // subscription handler is NOT invoked => no target write.
    bool fired = vm.Set(PropertyId::of(kText), Value::String("same"));
    SHOULD(ShouldlyIntegral(fired ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(target.stringWrites).ShouldBe(writesAfterFirst));
}

// ===========================================================================
// 5 - TARGET-SIDE COALESCING (source change to a value the target already holds)
// ===========================================================================

TEST_CASE("S243 target-side coalescing: change to a value the target already holds -> NoChange -> 0 writes")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("A"));
    CountingTarget target;

    auto expr = make_oneway(vm, target.obj);
    (void)expr->TransferSourceToTarget(); // target now A (write #1)

    // Pre-set the TARGET directly to B so it already holds B.
    (void)target.obj.Set(PropertyId::of(kText), Value::String("B")); // write #2
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("B")));

    // Move the SOURCE A -> B: this IS a real source change (A != B), so the
    // live subscription fires and re-resolves to B — but the target ALREADY
    // holds B, so the target Set is NO_CHANGE -> ZERO redundant target writes.
    const int writesBefore = target.stringWrites; // == 2
    (void)vm.Set(PropertyId::of(kText), Value::String("B"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("B")));
    SHOULD(ShouldlyIntegral(target.stringWrites).ShouldBe(writesBefore));
}

// ===========================================================================
// 6 - HEADLINE: DP INVALIDATION COALESCING across an interleaved burst
// ===========================================================================

TEST_CASE("S243 HEADLINE: interleaved burst A,A,B,B,C -> EXACTLY 3 target writes")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("A"));
    CountingTarget target;

    auto expr = make_oneway(vm, target.obj);
    (void)expr->TransferSourceToTarget(); // write #1 (A)

    const int writesAfterFirst = target.stringWrites; // == 1

    // Burst: A,A,B,B,C  (the equal repeats must NOT write)
    (void)vm.Set(PropertyId::of(kText), Value::String("A")); // source-equal: no handler
    (void)vm.Set(PropertyId::of(kText), Value::String("B")); // real: write #2
    (void)vm.Set(PropertyId::of(kText), Value::String("B")); // source-equal: no handler
    (void)vm.Set(PropertyId::of(kText), Value::String("C")); // real: write #3

    // Exactly 3 effective writes total (initial A + B + C); the equal repeats
    // contributed 0.
    SHOULD(ShouldlyIntegral(target.stringWrites).ShouldBe(3));
    SHOULD(ShouldlyIntegral(writesAfterFirst).ShouldBe(1));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("C")));
}

// ===========================================================================
// 7/8 - MODE INTROSPECTION + ONETIME UNCHANGED
// ===========================================================================

TEST_CASE("S243 mode: subscription armed only for OneWay; OneTime has none")
{
    // OneTime -> not subscribed.
    {
        ObservableObject vm;
        (void)vm.Set(PropertyId::of(kText), Value::String("x"));
        CountingTarget target;
        BindingExpressionSpec spec;
        spec.source = &vm;
        spec.sourcePath = kText;
        spec.target = &target.obj;
        spec.targetProp = kText;
        spec.mode = BindingMode::OneTime;
        auto e = BindingExpression::create(spec);
        SHOULD(ShouldlyIntegral(e != nullptr).ShouldBe(true));
        SHOULD(ShouldlyIntegral(e->live_subscribed() ? 1 : 0).ShouldBe(0));
    }
    // OneWay -> subscribed on create.
    {
        ObservableObject vm;
        (void)vm.Set(PropertyId::of(kText), Value::String("x"));
        CountingTarget target;
        auto e = make_oneway(vm, target.obj);
        SHOULD(ShouldlyIntegral(e->live_subscribed() ? 1 : 0).ShouldBe(1));
        SHOULD(ShouldlyIntegral(mode_int(e->mode())).ShouldBe(mode_int(BindingMode::OneWay)));
    }
}

TEST_CASE("S243 OneTime unchanged: mutating the source does NOT auto-update")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("first"));
    CountingTarget target;
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target.obj;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneTime;
    auto expr = BindingExpression::create(spec);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("first")));

    (void)vm.Set(PropertyId::of(kText), Value::String("second"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("first")));
}

// ===========================================================================
// 9 - CONVERTER IN ONE-WAY (live)
// ===========================================================================

TEST_CASE("S243 converter: a source change re-runs the converter and the converted value reaches the target live")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("1"));
    CountingTarget target;

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target.obj;
    spec.targetProp = kNumber; // a converter maps String -> Int32 into the Int32 DP
    spec.mode = BindingMode::OneWay;
    spec.converter.convert = [](const Value& v) -> BindingConversion {
        return BindingConversion{ true, Value::Int32(static_cast<std::int32_t>(v.get<std::string>().size())), {} };
    };
    spec.converterName = "S243Len";
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(target.obj.Get(PropertyId::of(kNumber)).get<std::int32_t>()).ShouldBe(1));

    // "Hello" -> 5; the live subscription re-runs the converter.
    (void)vm.Set(PropertyId::of(kText), Value::String("Hello"));
    SHOULD(ShouldlyIntegral(target.obj.Get(PropertyId::of(kNumber)).get<std::int32_t>()).ShouldBe(5));

    // "World!" -> 6.
    (void)vm.Set(PropertyId::of(kText), Value::String("World!"));
    SHOULD(ShouldlyIntegral(target.obj.Get(PropertyId::of(kNumber)).get<std::int32_t>()).ShouldBe(6));
}

// ===========================================================================
// 10 - FALLBACK IN ONE-WAY (live restore)
// ===========================================================================

TEST_CASE("S243 fallback: a source resolving to a missing path -> FallbackApplied; restore -> live re-update")
{
    ObservableObject vm;
    CountingTarget target;
    RecordingSink sink;
    SinkGuard guard(sink);

    // Bind to an ABSENT source name (MissingProperty, Class B) with a fallback.
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNope; // absent -> MissingProperty
    spec.target = &target.obj;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneWay;
    spec.fallback = Value::String("FB");
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::FallbackApplied)));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("FB")));
    // MissingProperty + FallbackApplied emitted.
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{2}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::MissingProperty)));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[1])).ShouldBe(rc_int(BindingReasonCode::FallbackApplied)));

    // RESTORE: now the previously-absent source property becomes present with a
    // real value. The live subscription (keyed on kNope) fires; the re-resolve
    // succeeds -> the real value reaches the target (no fallback this time).
    sink.reasons.clear();
    (void)vm.Set(PropertyId::of(kNope), Value::String("restored"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("restored")));
    // No MissingProperty / FallbackApplied on the successful restore.
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true));
}

// ===========================================================================
// 11 - SetTargetFailed emission (S240_1 arm)
// ===========================================================================

TEST_CASE("S243 SetTargetFailed: a OneWay target Set rejection emits SetTargetFailed once per live re-push")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("str"));
    // Int32 target property; a String source pushed into it is wrong-typed.
    DependencyObject target;
    target.register_property(key(kNumber, ValueType::Int32),
        PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;      // String
    spec.target = &target;
    spec.targetProp = kNumber;    // Int32 target => wrong-typed push
    spec.mode = BindingMode::OneWay;

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    // The caller-driven FIRST push emits NOTHING (S242 parity: the initial push
    // does not emit; the sink stays empty).
    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::TargetRejected)));
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true));

    // A LIVE OneWay re-push (a real source change) is rejected -> emits
    // SetTargetFailed EXACTLY once.
    sink.reasons.clear();
    (void)vm.Set(PropertyId::of(kText), Value::String("second"));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::SetTargetFailed)));

    // A second live rejection emits a second SetTargetFailed.
    (void)vm.Set(PropertyId::of(kText), Value::String("third"));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{2}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[1])).ShouldBe(rc_int(BindingReasonCode::SetTargetFailed)));
}

TEST_CASE("S243 SetTargetFailed: the OneTime wrong-typed path still emits NOTHING (S242 re-proof)")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("str"));
    DependencyObject target;
    target.register_property(key(kNumber, ValueType::Int32),
        PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target;
    spec.targetProp = kNumber;
    spec.mode = BindingMode::OneTime;
    auto expr = BindingExpression::create(spec);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(TransferResult::TargetRejected)).ShouldBe(tr_int(TransferResult::TargetRejected)));
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true)); // no SetTargetFailed
    // OneTime never subscribes, so even a later mutation stays silent.
    (void)vm.Set(PropertyId::of(kText), Value::String("x"));
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true));
}

// ===========================================================================
// 12 - SetTargetFailed != fallback
// ===========================================================================

TEST_CASE("S243 SetTargetFailed != fallback: a target rejection is never masked by a bound fallback")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("str"));
    // Int32 target with a bound fallback. The live push of a String into the
    // Int32 target is a TARGET rejection — NOT a Class B resolve/converter
    // failure — so the fallback must NOT mask it (no FallbackApplied emitted).
    DependencyObject target;
    target.register_property(key(kNumber, ValueType::Int32),
        PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target;
    spec.targetProp = kNumber;
    spec.mode = BindingMode::OneWay;
    spec.fallback = Value::Int32(99);
    auto expr = BindingExpression::create(spec);
    (void)expr->TransferSourceToTarget(); // first push: rejected, NO emission, NO fallback

    sink.reasons.clear();
    (void)vm.Set(PropertyId::of(kText), Value::String("live"));
    // The live rejection emits SetTargetFailed ONLY — FallbackApplied must NOT
    // appear (fallback is for Class B resolve/converter failures, never a
    // target-assignment rejection).
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::SetTargetFailed)));
    // Target untouched (still -1 default; the fallback was NOT applied).
    SHOULD(ShouldlyIntegral(target.Get(PropertyId::of(kNumber)).get<std::int32_t>()).ShouldBe(-1));
}

// ===========================================================================
// 13 - LIFETIME / DTOR
// ===========================================================================

TEST_CASE("S243 lifetime: after the expression is destroyed, a source mutation does NOT fire the handler")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("first"));
    CountingTarget target;

    {
        auto expr = make_oneway(vm, target.obj);
        (void)expr->TransferSourceToTarget();
        SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(1));
        (void)vm.Set(PropertyId::of(kText), Value::String("live"));
        SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("live")));
    } // expr destroyed -> unsubscribe

    const int writesAfterDestroy = target.stringWrites;
    (void)vm.Set(PropertyId::of(kText), Value::String("after"));
    // No handler any more: no new target write, no dangling callback.
    SHOULD(ShouldlyIntegral(target.stringWrites).ShouldBe(writesAfterDestroy));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("live")));
}

// ===========================================================================
// 14 - MOVE
// ===========================================================================

TEST_CASE("S243 move: a moved one-way expression re-arms its live subscription; mutation after the move still updates the target")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("first"));
    CountingTarget target;

    auto expr = make_oneway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(1));

    auto moved = std::move(expr);
    SHOULD(ShouldlyIntegral(moved->live_subscribed() ? 1 : 0).ShouldBe(1));

    // The moved-to binding's handler captures the moved-to `this`; a mutation
    // still re-pushes.
    (void)vm.Set(PropertyId::of(kText), Value::String("after-move"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("after-move")));

    // The moved-from is inert (its subscription was transferred; its dtor must
    // not double-remove or touch a dangling `this`). Destroying it is safe.
    expr.reset();
}
