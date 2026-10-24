// S242 — BindingExpression (dep:S241). Sprint 6.
//
// Proves the S242 headline against the REAL vaml::BindingExpression:
//   source/target, mode one/one-time/two-way, converter, fallback value, and the
//   first source->target transfer. Registers S242-unique object-scoped DPs on
//   real DependencyObject targets and reads from real ObservableObject sources
//   (via the S241 reader through the S240 evaluator).
//
// Suite map:
//   1  OneTime: initial transfer APPLIED (source String -> target String).
//   2  OneTime resolve-once: after `create`, mutating the source does NOT
//      re-transfer (no live subscription — that is S243/S244); target unchanged.
//   3  mode() + to_binding_direction mapping (OneTime/OneWay/TwoWay).
//   4  Fallback on MissingProperty (absent source prop) -> FallbackApplied.
//   5  Fallback on NullIntermediate (nested null) -> FallbackApplied.
//   6  No fallback + MissingProperty -> ResolveFailed, target untouched, only
//      the resolve reason emitted (no FallbackApplied).
//   7  Converter: converts String -> Int32 into a target Int32 prop -> APPLIED.
//   8  Converter failure (ok=false) -> ConverterFailed; with fallback ->
//      FallbackApplied.
//   9  Class A malformed path -> create returns nullptr, MalformedPath emitted,
//      NO fallback applied (Class A never falls back).
//   10 Target already == value -> NoChange.
//   11 Wrong-typed target push -> TargetRejected (returned, recorded; the
//      SetTargetFailed emission is S243's arm — NOT emitted at S242).
//   12 Source null / non-ObservableObject -> NotReadable resolve reason +
//      fallback path.
//   13 bindingId monotonic + distinct across create() calls.
//   14 RecordingSink receives the exact reasonCode per arm (MissingProperty /
//      NullIntermediate / NotReadable / ConverterFailed / FallbackApplied /
//      MalformedPath).
//   15 Empty/whitespace path -> EmptyPath (Class A), create nullptr.
//   16 Converter fails AND fallback present -> FallbackApplied (no throw).
//
// S242-unique property names (no global/member-name pollution).

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
using vaml::to_binding_direction;

namespace {

// ---- S242-unique names ----
const char* kNameText    = "S242Text";    // source String / target String
const char* kNameNumber  = "S242Number";  // source Int32 / target Int32
const char* kNameEmpty   = "S242Empty";   // source (null-valued) / target
const char* kNameNope    = "S242Nope";    // absent source name (MissingProperty)
const char* kNameDeep    = "S242Deep";    // nested target for a chain

PropertyKey key(const std::string& name, ValueType t)
{
    return PropertyKey(name, t, "S242Test", PropertyFlags::None);
}

DependencyObject make_target()
{
    DependencyObject obj;
    obj.register_property(key(kNameText, ValueType::String),
        PropertyMetadata::WithDefault(Value::String("DEFAULT_TEXT"), PropertyFlags::None));
    obj.register_property(key(kNameNumber, ValueType::Int32),
        PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
    obj.register_property(key(kNameEmpty, ValueType::Null),
        PropertyMetadata::WithDefault(Value::Null(), PropertyFlags::None));
    obj.register_property(key(kNameDeep, ValueType::String),
        PropertyMetadata::WithDefault(Value::String("DEFAULT_DEEP"), PropertyFlags::None));
    return obj;
}

// RAII guard: swaps in the recorder, restores the previous sink at scope exit
// (the process-global sink must be restored so tests can run in any order /
// against the real binding engine).
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
int dir_int(BindingDirection d) { return static_cast<int>(d); }
int mode_int(BindingMode m) { return static_cast<int>(m); }

} // namespace

// ===========================================================================
// 1 - FIRST TRANSFER (OneTime)
// ===========================================================================

TEST_CASE("S242 OneTime: create + initial transfer APPLIES String source -> String target")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameText), Value::String("Hello"));
    DependencyObject target = make_target();

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameText; // same name on target is fine (object-scoped)
    spec.mode = BindingMode::OneTime;

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::Applied)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("Hello")));
}

// ===========================================================================
// 2 - ONETIME RESOLVE-ONCE (no live re-transfer)
// ===========================================================================

TEST_CASE("S242 OneTime: mutating the source after transfer does NOT re-push (no live subscription)")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameText), Value::String("first"));
    DependencyObject target = make_target();

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::OneTime;

    auto expr = BindingExpression::create(spec);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("first")));

    // Poke the source; OneTime must NOT re-transfer on its own (S243/S244 wire it).
    (void)vm.Set(PropertyId::of(kNameText), Value::String("second"));
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("first")));
}

// ===========================================================================
// 3 - MODE CARRIED + DIRECTION MAPPING
// ===========================================================================

TEST_CASE("S242 mode: create carries the mode; to_binding_direction maps correctly")
{
    SHOULD(ShouldlyIntegral(dir_int(to_binding_direction(BindingMode::OneTime))).ShouldBe(dir_int(BindingDirection::OneTime)));
    SHOULD(ShouldlyIntegral(dir_int(to_binding_direction(BindingMode::OneWay))).ShouldBe(dir_int(BindingDirection::OneWay)));
    SHOULD(ShouldlyIntegral(dir_int(to_binding_direction(BindingMode::TwoWay))).ShouldBe(dir_int(BindingDirection::TwoWay)));

    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameText), Value::String("x"));
    DependencyObject target = make_target();

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::TwoWay; // S244 wire; mode carried now, tested now

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(mode_int(expr->mode())).ShouldBe(mode_int(BindingMode::TwoWay)));
    // The first push reuses the same OneTime-compatible transfer (S243/S244 build on it).
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("x")));
}

// ===========================================================================
// 4/5/6 - FALLBACK (Class B) + the resolve reasons
// ===========================================================================

TEST_CASE("S242 fallback: MissingProperty source -> FallbackApplied, target gets the fallback value")
{
    ObservableObject vm; // never sets kNameNope
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameNope; // absent -> MissingProperty (Class B)
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::OneTime;
    spec.fallback = Value::String("FALLBACK");

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::FallbackApplied)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("FALLBACK")));
}

TEST_CASE("S242 fallback: NullIntermediate source -> FallbackApplied")
{
    // Outer holds a null-valued property, and we bind to "<empty>.<text>" so
    // the chain hits an intermediate null.
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameEmpty), Value::Null());
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = std::string(kNameEmpty) + "." + kNameText; // intermediate null
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::OneTime;
    spec.fallback = Value::String("FB");

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::FallbackApplied)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("FB")));
}

TEST_CASE("S242 fallback: MissingProperty WITHOUT a fallback -> ResolveFailed, target untouched, no FallbackApplied")
{
    ObservableObject vm;
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameNope;
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::OneTime;

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::ResolveFailed)));
    // Target untouched: still the default.
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("DEFAULT_TEXT")));
}

// ===========================================================================
// 7/8 - CONVERTER
// ===========================================================================

TEST_CASE("S242 converter: converts String source -> Int32 target, APPLIED")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameText), Value::String("42"));
    DependencyObject target = make_target();

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameNumber; // Int32 target
    spec.mode = BindingMode::OneTime;
    spec.converter.convert = [](const Value&) -> BindingConversion {
        // A fixed conversion for the test (String -> Int32).
        return BindingConversion{ true, Value::Int32(7), {} };
    };
    spec.converterName = "S242ToInt";

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::Applied)));
    SHOULD(ShouldlyIntegral(target.Get(PropertyId::of(kNameNumber)).get<std::int32_t>()).ShouldBe(7));
}

TEST_CASE("S242 converter: failure (ok=false) -> ConverterFailed; with fallback -> FallbackApplied")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameText), Value::String("x"));
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameNumber;
    spec.mode = BindingMode::OneTime;
    spec.converter.convert = [](const Value&) -> BindingConversion {
        return BindingConversion{ false, Value::Null(), "converter refused" };
    };
    spec.converterName = "S242Fail";
    spec.fallback = Value::Int32(99);

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::FallbackApplied)));
    SHOULD(ShouldlyIntegral(target.Get(PropertyId::of(kNameNumber)).get<std::int32_t>()).ShouldBe(99));
}

// ===========================================================================
// 9 - CLASS A GATE (create returns nullptr, no fallback, emits MalformedPath)
// ===========================================================================

TEST_CASE("S242 Class A: malformed path -> create returns nullptr, emits MalformedPath, NO fallback applied")
{
    ObservableObject vm;
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = "S242Text.."; // TrailingDot/ConsecutiveDot Class A
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::OneTime;
    spec.fallback = Value::String("FB"); // must NOT be applied (Class A never falls back)

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr == nullptr).ShouldBe(true));
    // Target untouched — nothing bound, nothing applied.
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("DEFAULT_TEXT")));
}

// ===========================================================================
// 10/11 - APPLIED / NOCHANGE / TARGETREJECTED (no SetTargetFailed emission)
// ===========================================================================

TEST_CASE("S242 transfer: target already == value -> NoChange")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameText), Value::String("same"));
    DependencyObject target = make_target();
    (void)target.Set(PropertyId::of(kNameText), Value::String("same")); // pre-set equal

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::OneTime;

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::NoChange)));
}

TEST_CASE("S242 transfer: wrong-typed push -> TargetRejected (returned + recorded, NOT emitted SetTargetFailed)")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameText), Value::String("str"));
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    // Push a String into an Int32 target property (value-type mismatch).
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameNumber; // Int32 target
    spec.mode = BindingMode::OneTime;

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::TargetRejected)));
    // The S240_1 SetTargetFailed arm is S243's — it must NOT have been emitted here.
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true));
}

// ===========================================================================
// 12 - NOTREADABLE RESOLVE + FALLBACK
// ===========================================================================

TEST_CASE("S242 resolve: null source (no ObservableObject) -> NotReadable + fallback when present")
{
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = nullptr; // -> not-readable
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::OneTime;
    spec.fallback = Value::String("NULL_FB");

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::FallbackApplied)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("NULL_FB")));
}

TEST_CASE("S242 resolve: null source WITHOUT fallback -> ResolveFailed, target untouched")
{
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = nullptr;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::OneTime;

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::ResolveFailed)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kNameText))).ShouldBe(Value::String("DEFAULT_TEXT")));
}

// ===========================================================================
// 13 - BINDING ID MONOTONIC / DISTINCT
// ===========================================================================

TEST_CASE("S242 id: bindingId is monotonic and distinct across create() calls")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameText), Value::String("a"));
    DependencyObject t1 = make_target();
    DependencyObject t2 = make_target();

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.mode = BindingMode::OneTime;
    spec.target = &t1;
    spec.targetProp = kNameText;
    auto e1 = BindingExpression::create(spec);
    spec.target = &t2;
    auto e2 = BindingExpression::create(spec);

    SHOULD(ShouldlyIntegral(e1 != nullptr && e2 != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(e1->binding_id()).ShouldNotBe(e2->binding_id()));
    // Increasing ordinal (strictly greater across two creates).
    SHOULD(ShouldlyIntegral(e1->binding_id() > 0).ShouldBe(true));
    SHOULD(ShouldlyIntegral(e2->binding_id() > e1->binding_id()).ShouldBe(true));
}

// ===========================================================================
// 14 - EXACT REASON CODES per arm (RecordingSink)
// ===========================================================================

TEST_CASE("S242 diagnostics: exact reason codes per failure arm")
{
    // MissingProperty (no fallback).
    {
        ObservableObject vm;
        DependencyObject target = make_target();
        RecordingSink sink;
        SinkGuard guard(sink);
        BindingExpressionSpec spec;
        spec.source = &vm;
        spec.sourcePath = kNameNope;
        spec.target = &target;
        spec.targetProp = kNameText;
        spec.mode = BindingMode::OneTime;
        auto e = BindingExpression::create(spec);
        (void)e->TransferSourceToTarget();
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::MissingProperty)));
    }
    // NullIntermediate (no fallback).
    {
        ObservableObject vm;
        (void)vm.Set(PropertyId::of(kNameEmpty), Value::Null());
        DependencyObject target = make_target();
        RecordingSink sink;
        SinkGuard guard(sink);
        BindingExpressionSpec spec;
        spec.source = &vm;
        spec.sourcePath = std::string(kNameEmpty) + ".X";
        spec.target = &target;
        spec.targetProp = kNameText;
        spec.mode = BindingMode::OneTime;
        auto e = BindingExpression::create(spec);
        (void)e->TransferSourceToTarget();
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::NullIntermediate)));
    }
    // NotReadable (null source, no fallback).
    {
        DependencyObject target = make_target();
        RecordingSink sink;
        SinkGuard guard(sink);
        BindingExpressionSpec spec;
        spec.source = nullptr;
        spec.sourcePath = kNameText;
        spec.target = &target;
        spec.targetProp = kNameText;
        spec.mode = BindingMode::OneTime;
        auto e = BindingExpression::create(spec);
        (void)e->TransferSourceToTarget();
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::NotReadable)));
    }
    // ConverterFailed (no fallback).
    {
        ObservableObject vm;
        (void)vm.Set(PropertyId::of(kNameText), Value::String("x"));
        DependencyObject target = make_target();
        RecordingSink sink;
        SinkGuard guard(sink);
        BindingExpressionSpec spec;
        spec.source = &vm;
        spec.sourcePath = kNameText;
        spec.target = &target;
        spec.targetProp = kNameNumber;
        spec.mode = BindingMode::OneTime;
        spec.converter.convert = [](const Value&) -> BindingConversion {
            return BindingConversion{ false, Value::Null(), "no" };
        };
        spec.converterName = "S242Fail";
        auto e = BindingExpression::create(spec);
        (void)e->TransferSourceToTarget();
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::ConverterFailed)));
    }
    // FallbackApplied follows a Class B reason (MissingProperty + fallback).
    {
        ObservableObject vm;
        DependencyObject target = make_target();
        RecordingSink sink;
        SinkGuard guard(sink);
        BindingExpressionSpec spec;
        spec.source = &vm;
        spec.sourcePath = kNameNope;
        spec.target = &target;
        spec.targetProp = kNameText;
        spec.mode = BindingMode::OneTime;
        spec.fallback = Value::String("FB");
        auto e = BindingExpression::create(spec);
        (void)e->TransferSourceToTarget();
        // MissingProperty (+ then) FallbackApplied.
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{2}));
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::MissingProperty)));
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[1])).ShouldBe(rc_int(BindingReasonCode::FallbackApplied)));
    }
    // Class A malformed path -> MalformedPath (emitted at create, no bind).
    {
        ObservableObject vm;
        DependencyObject target = make_target();
        RecordingSink sink;
        SinkGuard guard(sink);
        BindingExpressionSpec spec;
        spec.source = &vm;
        spec.sourcePath = "S242Text..";
        spec.target = &target;
        spec.targetProp = kNameText;
        spec.mode = BindingMode::OneTime;
        auto e = BindingExpression::create(spec);
        SHOULD(ShouldlyIntegral(e == nullptr).ShouldBe(true));
        SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
        SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::MalformedPath)));
    }
}

// ===========================================================================
// 15/16 - EMPTY PATH (Class A) + CONVERTER FAILS WITH FALLBACK present
// ===========================================================================

TEST_CASE("S242 Class A: empty/whitespace path -> EmptyPath, create nullptr")
{
    ObservableObject vm;
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = "   "; // whitespace-only -> Empty
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.mode = BindingMode::OneTime;

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr == nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::EmptyPath)));
}

TEST_CASE("S242 converter fails AND fallback present -> FallbackApplied (never throws)")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kNameText), Value::String("x"));
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameNumber;
    spec.mode = BindingMode::OneTime;
    spec.converter.convert = [](const Value&) -> BindingConversion {
        throw std::runtime_error("boom"); // a throwing converter must be recovered
    };
    spec.converterName = "S242Throws";
    spec.fallback = Value::Int32(5);

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::FallbackApplied)));
    SHOULD(ShouldlyIntegral(target.Get(PropertyId::of(kNameNumber)).get<std::int32_t>()).ShouldBe(5));
}
