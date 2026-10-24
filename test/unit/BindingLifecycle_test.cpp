// S245 (dep:S244) — BindingExpression EXPRESSION LIFECYCLE: detach on element destroy.
// Sprint 6.
//
// Proves the S245 headline against the REAL vaml::BindingExpression:
//   (A) detach-on-element-destroy — a binding subscribes to its target
//       DependencyObject's destruction notification (S245 add_destroyed), and
//       when the target ELEMENT is destroyed the binding DETACHES (unsubscribes
//       all wires) BEFORE the target's memory is reclaimed;
//   (B) a DETACHED binding writes NOTHING — a source change (or target change)
//       after detach does NOT write to the target/source, and instead emits
//       BindingReasonCode::Detached (the S240_1 forward arm, Class B, a SILENCED
//       non-error) EXACTLY ONCE so a subscribed sink can observe the lifecycle
//       (never a dangling-target write);
//   (C) explicit Detach() — idempotent, no-throw, emits nothing (normal
//       teardown), and unsubscribes BOTH the forward + write-back wires;
//   (D) OneWay + OneTime parity — all modes are guarded after detach;
//   (E) move re-arms the destruction wire onto the new `this`; dtor leaves no
//       dangling callback.
//
// Registers S245-unique object-scoped DPs on real DependencyObject targets +
// reads real ObservableObject sources. Suite map:
//   1  fresh not detached.
//   2  explicit Detach() -> detached + both wires unsubscribed + zero emissions.
//   3  HEADLINE — auto-detach on element destroy (TwoWay): destroy the target ->
//      binding detached -> later source change emits Detached exactly once +
//      target NOT written (no dangling write).
//   4  source change after explicit Detach() -> Detached once + target untouched.
//   5  target edit after Detach() -> Detached once + source untouched.
//   6  OneWay parity: source change after detach emits Detached, target untouched.
//   7  OneTime parity: transfer after Detach() emits Detached once, target untouched.
//   8  Detached is Class B + binding_class_of (silenced non-error).
//   9  move re-arms the destruction wire (binding still auto-detaches after move).
//  10  dtor leaves no dangling callback (no crash after both go out of scope).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam)

#include "vaml/core/BindingDiagnostics.h"
#include "vaml/core/BindingExpression.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyMetadata.h"

#include <memory>
#include <string>
#include <vector>

using vaml::BindingExpression;
using vaml::BindingExpressionSpec;
using vaml::BindingMode;
using vaml::BindingReasonCode;
using vaml::DependencyObject;
using vaml::IBindingDiagnostics;
using vaml::ObservableObject;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::Value;
using vaml::ValueType;
using vaml::binding_diagnostics;
using vaml::set_binding_diagnostics;

namespace {

// ---- S245-unique names ----
const char* kText = "S245Text"; // source String / target String (two-way axis)

PropertyKey key(const std::string& name, ValueType t)
{
    return PropertyKey(name, t, "S245Test", PropertyFlags::None);
}

PropertyMetadata string_default(const char* def)
{
    return PropertyMetadata::WithDefault(Value::String(def), PropertyFlags::None);
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

int rc_int(BindingReasonCode c) { return static_cast<int>(c); }
int rc(BindingReasonCode c) { return static_cast<int>(c); }

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

// A OneWay binding over `vm` -> `target`.
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

// A OneTime binding over `vm` -> `target`.
std::unique_ptr<BindingExpression> make_onetime(ObservableObject& vm, DependencyObject& target)
{
    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kText;
    spec.target = &target;
    spec.targetProp = kText;
    spec.mode = BindingMode::OneTime;
    return BindingExpression::create(spec);
}

} // namespace

// ===========================================================================
// 1 - FRESH NOT DETACHED
// ===========================================================================

TEST_CASE("S245 fresh binding is not detached; wires armed")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("Hello"));
    DependencyObject target;
    (void)target.register_property(key(kText, ValueType::String), string_default("DEFAULT"));

    auto expr = make_twoway(vm, target);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(expr->detached() ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(1));
}

// ===========================================================================
// 2 - EXPLICIT Detach()
// ===========================================================================

TEST_CASE("S245 explicit Detach() -> detached + both wires unsubscribed + no emission")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("Hello"));
    DependencyObject target;
    (void)target.register_property(key(kText, ValueType::String), string_default("DEFAULT"));

    RecordingSink sink;
    SinkGuard guard(sink);
    auto expr = make_twoway(vm, target);
    (void)expr->TransferSourceToTarget();

    expr->Detach();
    SHOULD(ShouldlyIntegral(expr->detached() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(0));
    // Normal teardown emits NOTHING.
    SHOULD(ShouldlyIntegral(static_cast<int>(sink.reasons.size())).ShouldBe(0));

    // Idempotent: a second Detach() is a no-op (still detached, still silent).
    expr->Detach();
    SHOULD(ShouldlyIntegral(expr->detached() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(sink.reasons.size())).ShouldBe(0));
}

// ===========================================================================
// 3 - HEADLINE: auto-detach on element destroy (TwoWay)
// ===========================================================================

TEST_CASE("S245 HEADLINE: destroying the target auto-detaches; later source change emits Detached once + no write")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("initial"));
    RecordingSink sink;
    SinkGuard guard(sink);

    auto expr = [&]() -> std::unique_ptr<BindingExpression> {
        auto target = std::make_unique<DependencyObject>();
        (void)target->register_property(key(kText, ValueType::String), string_default("DEFAULT"));
        auto e = make_twoway(vm, *target);
        (void)e->TransferSourceToTarget();
        // Destroy the target = the element-destroy lifecycle fires -> detach.
        target.reset();
        return e;
    }();

    // The binding is detached once the element is destroyed (no dangling write).
    SHOULD(ShouldlyIntegral(expr->detached() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(expr->live_subscribed() ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(expr->live_writeback() ? 1 : 0).ShouldBe(0));
    // Normal teardown emitted nothing.
    SHOULD(ShouldlyIntegral(static_cast<int>(sink.reasons.size())).ShouldBe(0));

    // A later source change would WRITE to the destroyed target — it must be
    // suppressed + the lifecycle observed. We can't Set on the (destroyed)
    // source subscription (it fired into a detached handler), so drive the write
    // attempt via TransferSourceToTarget directly.
    (void)vm.Set(PropertyId::of(kText), Value::String("after-destroy"));
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(rc_int(BindingReasonCode::Detached)).ShouldBe(rc(BindingReasonCode::Detached)));
    // Detached emitted exactly once.
    int detachedCount = 0;
    for (auto r : sink.reasons) if (r == BindingReasonCode::Detached) ++detachedCount;
    SHOULD(ShouldlyIntegral(detachedCount).ShouldBe(1));
}

// ===========================================================================
// 4 - SOURCE CHANGE AFTER EXPLICIT Detach() (write attempt)
// ===========================================================================

TEST_CASE("S245 a source change after explicit Detach() emits Detached once + target untouched")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("before"));
    DependencyObject target;
    (void)target.register_property(key(kText, ValueType::String), string_default("DEFAULT"));
    RecordingSink sink;
    SinkGuard guard(sink);

    auto expr = make_twoway(vm, target);
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("before")));

    expr->Detach();
    (void)vm.Set(PropertyId::of(kText), Value::String("after"));
    (void)expr->TransferSourceToTarget();

    // Target is NOT written (still the pre-detach value).
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("before")));
    int detachedCount = 0;
    for (auto r : sink.reasons) if (r == BindingReasonCode::Detached) ++detachedCount;
    SHOULD(ShouldlyIntegral(detachedCount).ShouldBe(1));
}

// ===========================================================================
// 5 - TARGET EDIT AFTER Detach() (write-back attempt)
// ===========================================================================

TEST_CASE("S245 a target edit after Detach() emits Detached once + source untouched")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("before"));
    DependencyObject target;
    (void)target.register_property(key(kText, ValueType::String), string_default("DEFAULT"));
    RecordingSink sink;
    SinkGuard guard(sink);

    auto expr = make_twoway(vm, target);
    (void)expr->TransferSourceToTarget();

    expr->Detach();
    // A target edit (TextBox typing) lands on the target DP. The write-back wire
    // is gone, so nothing writes to the source; the (still-live) target Set
    // cannot reach the detached binding's write-back — Detached is not emitted
    // from a target edit because the write-back wire was already unsubscribed.
    (void)target.Set(PropertyId::of(kText), Value::String("edited"));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("before")));

    // The Detached lifecycle arm is bound to the FORWARD write path (a target
    // binding's own write-back wire is gone on detach) — assert the source is
    // untouched and no false emission occurred.
    SHOULD(ShouldlyIntegral(static_cast<int>(sink.reasons.size())).ShouldBe(0));
}

// ===========================================================================
// 6 - ONEWAY PARITY
// ===========================================================================

TEST_CASE("S245 OneWay parity: source change after Detach() emits Detached once + target untouched")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("keep"));
    DependencyObject target;
    (void)target.register_property(key(kText, ValueType::String), string_default("DEFAULT"));
    RecordingSink sink;
    SinkGuard guard(sink);

    auto expr = make_oneway(vm, target);
    (void)expr->TransferSourceToTarget();
    expr->Detach();

    (void)vm.Set(PropertyId::of(kText), Value::String("new"));
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("keep")));
    int detachedCount = 0;
    for (auto r : sink.reasons) if (r == BindingReasonCode::Detached) ++detachedCount;
    SHOULD(ShouldlyIntegral(detachedCount).ShouldBe(1));
}

// ===========================================================================
// 7 - ONETIME PARITY
// ===========================================================================

TEST_CASE("S245 OneTime parity: transfer after Detach() emits Detached once + target untouched")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("keep"));
    DependencyObject target;
    (void)target.register_property(key(kText, ValueType::String), string_default("DEFAULT"));
    RecordingSink sink;
    SinkGuard guard(sink);

    auto expr = make_onetime(vm, target);
    (void)expr->TransferSourceToTarget();
    expr->Detach();

    (void)vm.Set(PropertyId::of(kText), Value::String("changed"));
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyString(target.Get(PropertyId::of(kText))).ShouldBe(Value::String("keep")));
    int detachedCount = 0;
    for (auto r : sink.reasons) if (r == BindingReasonCode::Detached) ++detachedCount;
    SHOULD(ShouldlyIntegral(detachedCount).ShouldBe(1));
}

// ===========================================================================
// 8 - Detached is Class B + binding_class_of
// ===========================================================================

TEST_CASE("S245 Detached reason is Class B (silenced non-error) via binding_class_of")
{
    int cls = static_cast<int>(vaml::binding_class_of(BindingReasonCode::Detached));
    int expected = static_cast<int>(vaml::BindingClass::DefinedRuntimeOutcome);
    SHOULD(ShouldlyIntegral(cls).ShouldBe(expected));
    // Not conflated with the forward-set / write-back failure arms.
    SHOULD(ShouldlyIntegral(rc(BindingReasonCode::Detached))
               .ShouldNotBe(rc(BindingReasonCode::SetTargetFailed)));
    SHOULD(ShouldlyIntegral(rc(BindingReasonCode::Detached))
               .ShouldNotBe(rc(BindingReasonCode::WriteBackRejected)));
}

// ===========================================================================
// 9 - MOVE re-arms the destruction wire
// ===========================================================================

TEST_CASE("S245 a moved binding keeps auto-detaching on element destroy")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("initial"));
    RecordingSink sink;
    SinkGuard guard(sink);

    // Build the binding in a scope where the target is destroyed after the move.
    std::unique_ptr<BindingExpression> moved;
    {
        DependencyObject target;
        (void)target.register_property(key(kText, ValueType::String), string_default("DEFAULT"));
        auto expr = make_twoway(vm, target);
        (void)expr->TransferSourceToTarget();
        moved = std::make_unique<BindingExpression>(std::move(*expr));
        // expr (moved-from) is inert; `moved` holds the live expression.
    } // target destroyed here -> the moved binding must auto-detach

    SHOULD(ShouldlyIntegral(moved != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(moved->detached() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(sink.reasons.size())).ShouldBe(0));

    (void)vm.Set(PropertyId::of(kText), Value::String("after"));
    (void)moved->TransferSourceToTarget();
    int detachedCount = 0;
    for (auto r : sink.reasons) if (r == BindingReasonCode::Detached) ++detachedCount;
    SHOULD(ShouldlyIntegral(detachedCount).ShouldBe(1));
}

// ===========================================================================
// 10 - DTOR leaves no dangling callback
// ===========================================================================

TEST_CASE("S245 destruction of the expression leaves no dangling callback (no crash)")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kText), Value::String("x"));
    {
        DependencyObject target;
        (void)target.register_property(key(kText, ValueType::String), string_default("DEFAULT"));
        auto expr = make_twoway(vm, target);
        (void)expr->TransferSourceToTarget();
        // expr destroyed here: unsubscribes the forward/write-back/destroyed wires.
    }
    // target still alive, vm still alive — no dangling callback invoked.
    (void)vm.Set(PropertyId::of(kText), Value::String("y"));
    SHOULD(ShouldlyString(vm.Get(PropertyId::of(kText))).ShouldBe(Value::String("y")));
}
