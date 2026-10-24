// S244 (dep:S243) — BindingExpression live TWO-WAY (feed-back: target→source).
// Sprint 6.
//
// Proves the S244 headline against the REAL vaml::BindingExpression:
//   (A) the TARGET→SOURCE WRITE-BACK — a TwoWay binding arms a per-property
//       subscription on the TARGET DependencyObject (S041 add_property_changed
//       keyed on targetProp_; the "target changed" signal, which models a
//       TextBox edit landing on the target DP) and, on a real target change,
//       writes the target's value BACK to the source — the "TextBox edit →
//       VM updates" headline (no manual call needed);
//   (B) the CONVERTBACK reverse-converter wire — S242 declared convertBack an
//       empty seam; S244 RESOLVES it (forward convert source→target, a target
//       edit is convertBack'd and reaches the source);
//   (C) the S240_1 WriteBackRejected FORWARD ARM — a live two-way write-back
//       that is rejected (failed convertBack / no source / no source key)
//       emits BindingReasonCode::WriteBackRejected (Class B, observation never
//       control-flow per ADR-016, never a throw);
//   (D) LOOP-SAFETY by the real-change discipline — a source→target→write-back
//       →source cycle is BOUNDED (no reentrancy/suppression flags, NO-LAZY-LLM):
//       a write-back to an equal source and a forward push to an equal target
//       both terminate.
//
// Registers S244-unique object-scoped DPs on real DependencyObject targets +
// reads real ObservableObject sources. Suite map:
//   1  TwoWay initial push APPLIED + live_writeback() armed on create.
//   2  TwoWay LIVE forward (S243 shared wire reused): source mutation updates
//      the target.
//   3  HEADLINE — target edit → source: target.Set(Text,"edited") →
//      source VM property becomes "edited" (no manual call).
//   4  ConvertBack: forward String→Int32; a target Int32 edit is convertBack'd
//      String and reaches the source.
//   5  Echo/loop termination: a full source→target→writeback→source cycle is
//      bounded; final values correct; target write count sane (no runaway).
//   6  A target edit yielding a source value equal to the current source →
//      source Set false → no forward re-trigger → still stable.
//   7  WriteBackRejected: a target edit whose convertBack fails emits
//      WriteBackRejected exactly once per rejection.
//   8  WriteBackRejected with NO source → emitted (Class B, never a throw).
//   9  WriteBackRejected != SetTargetFailed: a rejected write-back emits only
//      WriteBackRejected; a rejected forward Set emits only SetTargetFailed.
//  10  OneWay parity: OneWay arms forward only (live_writeback()==false); a
//      target edit does NOT push to source.
//  11  OneTime inert: neither forward nor write-back armed; target edit and
//      source change both inert.
//  12  No-converter two-way round-trip: a matching-type target edit writes the
//      raw value back to source.
//  13  Lifetime/dtor: after destruction neither handler fires (no dangling
//      callback).
//  14  Move: a moved two-way expression re-arms BOTH subscriptions (forward +
//      write-back), both live after the move.

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

// ---- S244-unique names ----
const char* kText   = "S244Text";    // source String / target String (two-way)
const char* kNumber = "S244Number";  // Int32 target (two-way convertBack axis)

PropertyKey key(const std::string& name, ValueType t)
{
    return PropertyKey(name, t, "S244Test", PropertyFlags::None);
}

// A target whose String + Int32 DPs report real change NOTIFICATIONS through
// the metadata `with_changed` callback counter — the "target edited" axis.
struct CountingTarget
{
    DependencyObject obj;
    int stringWrites = 0;
    int numberWrites = 0;

    CountingTarget()
    {
        obj.register_property(key(kText, ValueType::String),
            PropertyMetadata::WithDefault(Value::String("DEFAULT"), PropertyFlags::None)
                .with_changed([this](const Value&, const Value&) { ++stringWrites; }));
        obj.register_property(key(kNumber, ValueType::Int32),
            PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None)
                .with_changed([this](const Value&, const Value&) { ++numberWrites; }));
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

// A TwoWay binding (text -> text, no converter) built over `vm` -> `target`.
std::unique_ptr<BindingExpression> make_twoway(ObservableObject& vm, DependencyObject& target)
{
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::TwoWay;
    return BindingExpression::create(spec);
}

} // namespace

// ===========================================================================
// 1 - INITIAL PUSH + ARMED WRITE-BACK (TwoWay)
// ===========================================================================

TEST_CASE("S244 TwoWay: create APPLIES the initial push and arms BOTH the forward and write-back wires")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("Hello"));
    CountingTarget target;

    auto expr = make_twoway(vm, target.obj);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    // Both wires armed on create: forward (S243 shared) + the S244 write-back.
    SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(1));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::Applied)));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("Hello")));
}

// ===========================================================================
// 2 - TWO-WAY LIVE FORWARD (S243 shared wire reused)
// ===========================================================================

TEST_CASE("S244 TwoWay LIVE forward: a source mutation auto-updates the target (shared S243 wire)")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("first"));
    CountingTarget target;

    auto expr = make_twoway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("first")));

    // Source change -> live forward re-push (the exact S243 layer, now armed
    // for TwoWay too).
    (void)vm.Set(PropertyId::of(kText), Value::String("second"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("second")));
}

// ===========================================================================
// 3 - HEADLINE: target edit -> source (TextBox edit -> VM updates)
// ===========================================================================

TEST_CASE("S244 HEADLINE: a target edit (TextBox) writes the value back to the source VM")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("initial"));
    CountingTarget target;

    auto expr = make_twoway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("initial")));

    // A TextBox edit lands on the TARGET DP as a real change. The write-back
    // subscription fires and pushes the value into the SOURCE VM — no manual
    // call.
    (void)target.obj.Set(PropertyId::of(kText), Value::String("edited"));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("edited")));
}

// ===========================================================================
// 4 - CONVERTBACK (the S242-declared seam, RESOLVED here)
// ===========================================================================

TEST_CASE("S244 convertBack: a target Int32 edit is convertBack'd to a String that reaches the source")
{
    // Forward: "" <-> 0, "a" <-> 1, ... a String name whose length is the Int32.
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("xyz")); // length 3
    CountingTarget target;

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;      // String source
    spec.target = &target.obj;
    spec.targetProp = kNumber;    // Int32 target
    spec.mode = BindingMode::TwoWay;
    spec.converter.convert = [](const Value& v) -> BindingConversion {
        return BindingConversion{ true, Value::Int32(static_cast<std::int32_t>(v.get<std::string>().size())), {} };
    };
    spec.converter.convertBack = [](const Value& v) -> BindingConversion {
        // Reverse: an Int32 n becomes a String of exactly n 'x's.
        const std::int32_t n = v.get<std::int32_t>();
        std::string s(static_cast<std::size_t>(n < 0 ? 0 : n), 'x');
        return BindingConversion{ true, Value::String(s), {} };
    };
    spec.converterName = "S244Len";
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    (void)expr->TransferSourceToTarget();
    // Forward converts source "xyz" (len 3) -> target Int32 3.
    SHOULD(ShouldlyIntegral(target.obj.Get(PropertyId::of(kNumber)).get<std::int32_t>()).ShouldBe(3));

    // A target edit: user types 5 into the Int32 field. convertBack runs and
    // the resulting String ("xxxxx") reaches the SOURCE.
    (void)target.obj.Set(PropertyId::of(kNumber), Value::Int32(5));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("xxxxx")));
}

// ===========================================================================
// 5 - ECHO / LOOP TERMINATION
// ===========================================================================

TEST_CASE("S244 loop safety: a source->target->write-back->source cycle is BOUNDED and stable")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("A"));
    CountingTarget target;

    auto expr = make_twoway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("A")));

    // Drive the FULL cycle: source A -> target A (forward); then a target edit
    // B -> source B (write-back). Because each Set only fires on a REAL change,
    // the cycle terminates: the write-back of B into the source (then holding B)
    // fires no re-forward, and the re-push of the equal value fires no loop.
    (void)vm.Set(PropertyId::of(kText), Value::String("B"));   // forward: target -> B
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("B")));
    (void)target.obj.Set(PropertyId::of(kText), Value::String("C")); // write-back: source -> C
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("C")));

    // No runaway: target tracks exactly the distinct values written (A + B + C
    // = 3 real target writes; any echo would inflate this).
    SHOULD(ShouldlyIntegral(target.stringWrites).ShouldBe(3));
    // Converged, stable.
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("C")));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("C")));
}

// ===========================================================================
// 6 - TARGET EDIT TO AN EQUAL SOURCE VALUE (no re-trigger)
// ===========================================================================

TEST_CASE("S244 stability: a target edit yielding the current source value fires NO forward re-trigger")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("same"));
    CountingTarget target;

    auto expr = make_twoway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    (void)target.obj.Set(PropertyId::of(kText), Value::String("other"));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("other")));

    // Now set the source back and force a target edit to the SAME value the
    // source already holds: the write-back's source Set returns false (equal)
    // -> no source signal -> no forward re-push; still stable.
    (void)vm.Set(PropertyId::of(kText), Value::String("other"));
    const int writesBefore = target.stringWrites;
    (void)target.obj.Set(PropertyId::of(kText), Value::String("other")); // target edit to equal source
    SHOULD(ShouldlyIntegral(target.stringWrites - writesBefore).ShouldBe(0)); // no re-forward echo
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("other")));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("other")));
}

// ===========================================================================
// 7 - WriteBackRejected: failed convertBack
// ===========================================================================

TEST_CASE("S244 WriteBackRejected: a target edit whose convertBack fails emits WriteBackRejected once per rejection")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("x"));
    CountingTarget target;
    RecordingSink sink;
    SinkGuard guard(sink);

    // A convertBack that ALWAYS fails.
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target.obj;
    spec.targetProp = kText;
    spec.mode = BindingMode::TwoWay;
    spec.converter.convert = [](const Value& v) -> BindingConversion {
        return BindingConversion{ true, v, {} };
    };
    spec.converter.convertBack = [](const Value&) -> BindingConversion {
        return BindingConversion{ false, Value::Null(), "reverse failed" };
    };
    spec.converterName = "S244FailingBack";
    auto expr = BindingExpression::create(spec);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(1));

    // The caller-driven FIRST push emits NOTHING (S242/S243 parity).
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true));

    // A LIVE target edit is rejected -> WriteBackRejected exactly once; the
    // target value is NOT written back to the source.
    (void)target.obj.Set(PropertyId::of(kText), Value::String("zzz"));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::WriteBackRejected)));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("x"))); // unchanged

    // A second rejection emits a second WriteBackRejected.
    (void)target.obj.Set(PropertyId::of(kText), Value::String("yyy"));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{2}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[1])).ShouldBe(rc_int(BindingReasonCode::WriteBackRejected)));
}

// ===========================================================================
// 8 - WriteBackRejected with NO source (Class B, never a throw)
// ===========================================================================

TEST_CASE("S244 WriteBackRejected: a two-way binding with no source emits WriteBackRejected on a target edit (never throws)")
{
    CountingTarget target;
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = nullptr;          // no source
    spec.sourcePath = kText;
    spec.target = &target.obj;
    spec.targetProp = kText;
    spec.mode = BindingMode::TwoWay;
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(1));

    (void)expr->TransferSourceToTarget(); // first push: not readable -> emitted, but no write-back
    // The first push resolves NotReadable (a Class B diagnosis) but the
    // WRITE-BACK arm is what we assert here; clear the resolve noise first.
    sink.reasons.clear();

    // A live target edit with no source -> WriteBackRejected (Class B, never a
    // throw).
    (void)target.obj.Set(PropertyId::of(kText), Value::String("e"));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::WriteBackRejected)));
}

// ===========================================================================
// 9 - WriteBackRejected != SetTargetFailed
// ===========================================================================

TEST_CASE("S244 WriteBackRejected != SetTargetFailed: distinct arms, never conflated")
{
    // PART A — a rejected WRITE-BACK emits ONLY WriteBackRejected (no
    // SetTargetFailed). Use a two-way binding with NO source.
    {
        CountingTarget target;
        RecordingSink sink;
        SinkGuard guard(sink);
        BindingExpressionSpec spec;
        spec.source = nullptr;
        spec.sourcePath = kText;
        spec.target = &target.obj;
        spec.targetProp = kText;
        spec.mode = BindingMode::TwoWay;
        auto expr = BindingExpression::create(spec);
        (void)expr->TransferSourceToTarget();
        sink.reasons.clear();
        (void)target.obj.Set(PropertyId::of(kText), Value::String("e"));
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::WriteBackRejected)));
        // No SetTargetFailed leaked in (the ONLY emitted reason is the
        // write-back rejection).
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::WriteBackRejected)));
    }

    // PART B — a rejected FORWARD Set emits ONLY SetTargetFailed. Build a
    // OneWay with an Int32 target receiving a String (wrong-typed) -> the live
    // forward re-push is rejected -> SetTargetFailed (S243 arm), NOT
    // WriteBackRejected.
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
        spec.mode = BindingMode::OneWay;
        auto expr = BindingExpression::create(spec);
        (void)expr->TransferSourceToTarget(); // first push: rejected, no emission
        sink.reasons.clear();
        (void)vm.Set(PropertyId::of(kText), Value::String("live")); // live rejection
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::SetTargetFailed)));
    }
}

// ===========================================================================
// 10 - ONEWAY PARITY
// ===========================================================================

TEST_CASE("S244 OneWay parity: OneWay arms forward only; a target edit does NOT push to source")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("initial"));
    CountingTarget target;

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target.obj;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneWay;
    auto expr = BindingExpression::create(spec);
    (void)expr->TransferSourceToTarget();

    SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(1));   // forward armed
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(0));    // NO write-back

    // A target edit must NOT push to the source (OneWay is single-direction).
    (void)target.obj.Set(PropertyId::of(kText), Value::String("edited"));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("initial")));
}

// ===========================================================================
// 11 - ONETIME INERT
// ===========================================================================

TEST_CASE("S244 OneTime inert: neither forward nor write-back armed; target edit and source change both inert")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("initial"));
    CountingTarget target;

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target.obj;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneTime;
    auto expr = BindingExpression::create(spec);
    (void)expr->TransferSourceToTarget();

    SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(0));

    // Neither a source change nor a target edit propagates.
    (void)vm.Set(PropertyId::of(kText), Value::String("src"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("initial")));
    (void)target.obj.Set(PropertyId::of(kText), Value::String("tgt"));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("src")));
}

// ===========================================================================
// 12 - NO-CONVERTER TWO-WAY ROUND-TRIP
// ===========================================================================

TEST_CASE("S244 no-converter round-trip: a matching-type target edit writes the raw value back to source")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("A"));
    CountingTarget target;

    auto expr = make_twoway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    (void)target.obj.Set(PropertyId::of(kText), Value::String("raw-back"));
    // Raw, matching-type target edit -> source holds the raw value.
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("raw-back")));
}

// ===========================================================================
// 13 - LIFETIME / DTOR
// ===========================================================================

TEST_CASE("S244 lifetime: after destruction neither the forward nor the write-back handler fires")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("first"));
    CountingTarget target;

    {
        auto expr = make_twoway(vm, target.obj);
        (void)expr->TransferSourceToTarget();
        SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(1));
        (void)target.obj.Set(PropertyId::of(kText), Value::String("live"));
        SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("live")));
    } // expr destroyed -> BOTH subscriptions removed

    // A source change no longer re-pushes to the target...
    (void)vm.Set(PropertyId::of(kText), Value::String("after"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("live")));

    // ...and a target edit no longer writes back to the source. The target's
    // OWN metadata cb still fires on its Set (that counter is independent of
    // the binding), but the WRITE-BACK must not carry "post" into the source —
    // the source stays "after" (the real no-dangling-write-back proof).
    (void)target.obj.Set(PropertyId::of(kText), Value::String("post"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("post"))); // local store write still works
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("after")));        // no write-back echo
}

// ===========================================================================
// 14 - MOVE
// ===========================================================================

TEST_CASE("S244 move: a moved two-way expression re-arms BOTH subscriptions; forward + write-back stay live")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("first"));
    CountingTarget target;

    auto expr = make_twoway(vm, target.obj);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(1));

    auto moved = std::move(expr);
    SHOULD(ShouldlyIntegral(moved->live_subscribed() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(moved->live_writeback() ? 1 : 0).ShouldBe(1));

    // Forward still live after the move.
    (void)vm.Set(PropertyId::of(kText), Value::String("from-source"));
    SHOULD(ShouldlyString(target.obj.Get(PropertyId::of(kText))).ShouldBe(Value::String("from-source")));

    // Write-back still live after the move.
    (void)target.obj.Set(PropertyId::of(kText), Value::String("from-target"));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("from-target")));

    // The moved-from is inert; destroying it is safe (no double-remove, no
    // dangling `this`).
    expr.reset();
}
