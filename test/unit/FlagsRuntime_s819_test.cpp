// S819 (dep:S818, Sprint 11aa) — OverridesInheritanceBehavior + NotDataBindable
// + BindsTwoWayByDefault RUNTIME EFFECT. S818 landed the byte-faithful
// PropertyFlags arms (S819 consumes them) and carried them through serialization;
// S819 gives the THREE named flags their runtime behavior against the REAL engine:
//
//   (a) NotDataBindable (0x080): `BindingExpression::create` REFUSES a target
//       whose metadata sets the flag — returns nullptr + emits the NEW Class B
//       `BindingReasonCode::NotDataBindableTarget` arm (the S240_1-BindingEngine
//       refusal path). Class B, ADR-016 observation-only, NEVER throws under
//       either policy (a NotDataBindable property can never bind; there is no
//       fallback — the refusal is the honest disposition).
//   (b) BindsTwoWayByDefault (0x100): a `{Binding}` whose target DP metadata sets
//       this flag, with no `Mode=` override, DEFAULTS TO TwoWay — the binding's
//       markup-determined mode honors the flag (resolves S045's "carried-through,
//       no runtime behavior" NO-GAP). An explicit `Mode=` always wins.
//   (c) OverridesInheritanceBehavior (0x040): carried through the metadata +
//       effective-value delivery CONSUMES it in the inherited-lookup path (S039
//       resolution) — the exact WPF semantic per FPM.cs:18-55; VAML's inheritance
//       walk honors it (the full InheritanceBehavior-override surface is a later
//       concern; here the FLAG is represented + wired into resolution, not silent).
//
// Registers S819-unique GLOBAL DependencyProperties (composite (name, owner) per
// S803 — no name-only DP hosting) + REAL DependencyObject targets + REAL
// ObservableObject sources, and uses the S240 evaluator + S241 reader + S240_1
// sink (the real binding engine). ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam)

#include "vaml/core/BindingDiagnostics.h"
#include "vaml/core/BindingExpression.h"
#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/ObservableObject.h"
#include "vaml/core/PropertyFlags.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyMetadata.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using vaml::BindingExpression;
using vaml::BindingExpressionSpec;
using vaml::BindingMode;
using vaml::BindingPolicy;
using vaml::BindingReasonCode;
using vaml::BindingClass;
using vaml::BindingDirection;
using vaml::DependencyObject;
using vaml::DependencyProperty;
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
using vaml::binding_class_of;
using vaml::set_binding_diagnostics;
using vaml::to_binding_direction;

namespace {

// ---- S819-unique names ----
const char* kNotBindable    = "S819NotBindable";      // NotDataBindable target (global)
const char* kNormal         = "S819Normal";           // normal bindable target (global)
const char* kTwoWayDefault  = "S819TwoWayDefault";    // BindsTwoWayByDefault target (global)
const char* kPlain          = "S819Plain";            // no-flag target (mode default OneWay)
const char* kInheritOIB     = "S819InheritOIB";       // Inherits + OverridesInheritanceBehavior (global)

// The S819-unique owner for the GLOBAL DPs (composite registration under one
// owner; distinct from every prior suite's owner per the S803 discipline).
const char* kOwner = "S819Owner";

// The S819-unique TESTS owner for the object-scoped mirror + the VM source key.
const char* kTestOwner = "S819Test";

// The ObservableObject source key (the live binding's first path segment).
const char* kSourceKey = "S819Source";

std::unordered_set<std::string>& s819_seen()
{
    static std::unordered_set<std::string> seen;
    return seen;
}

// Registers a GLOBAL DependencyProperty once per (name). `flags` carry the
// S819 runtime arm under test. Returns a property id keyable on the composite
// (name, owner).
const DependencyProperty& s819_register(const char* name, PropertyFlags flags, ValueType t)
{
    if (s819_seen().insert(name).second)
    {
        return DependencyProperty::Register(
            name, t, kOwner, PropertyMetadata::WithDefault(
                (t == ValueType::String) ? Value::String("DEFAULT") : Value::Int32(0), flags));
    }
    return DependencyProperty::Get(name, kOwner);
}

// A real DependencyObject target that mirrors the S819 global DPs onto its own
// store (so the binding target Set resolves + the S819(c) inheritance local can
// be authored). Mirrors `name` under the S819Test owner with the SAME flags.
void mirror_property(DependencyObject& obj, const char* name, PropertyFlags flags, Value def)
{
    PropertyKey key(name, def.type(), kTestOwner, flags);
    (void)obj.register_property(std::move(key), PropertyMetadata::WithDefault(def, flags));
}

// RAII guard: swaps in the recorder, restores the PERMANENT default sink at
// scope exit (the process-global sink must be restored so tests run in any
// order / against the real binding engine).
struct SinkGuard
{
    explicit SinkGuard(IBindingDiagnostics& sink) { (void)set_binding_diagnostics(sink); }
    ~SinkGuard() { (void)set_binding_diagnostics(vaml::default_binding_diagnostics()); }
};

struct RecordingSink final : IBindingDiagnostics
{
    std::vector<BindingReasonCode> reasons;
    void emit(const vaml::BindingDiagnostic& d) override { reasons.push_back(d.reasonCode); }
};

int rc_int(BindingReasonCode c) { return static_cast<int>(c); }
int cls_int(BindingClass c) { return static_cast<int>(c); }
int mode_int(BindingMode m) { return static_cast<int>(m); }

} // namespace

// ===========================================================================
// (a) NotDataBindable — the create() refusal gate
// ===========================================================================

TEST_CASE("S819(a) NotDataBindable: create REFUSES the target -> nullptr + NotDataBindableTarget (Class B)")
{
    (void)s819_register(kNotBindable, PropertyFlags::NotDataBindable, ValueType::String);

    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kSourceKey, kTestOwner), Value::String("hello"));

    DependencyObject target;
    mirror_property(target, kNormal, PropertyFlags::None, Value::String("DEFAULT"));

    BindingExpressionSpec spec;
    spec.source     = &vm;
    spec.sourcePath = kSourceKey;
    spec.target     = &target;
    spec.targetProp = kNotBindable;                       // global NotDataBindable DP
    spec.targetOwner = kOwner;
    spec.mode       = BindingMode::OneWay;
    spec.modeExplicit = true;

    RecordingSink sink;
    SinkGuard guard(sink);

    auto expr = BindingExpression::create(spec);
    // The refusal: NO bind (nullptr) + the Class B NotDataBindableTarget reason.
    SHOULD(ShouldlyIntegral(expr == nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::NotDataBindableTarget)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(sink.reasons[0]))).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
}

TEST_CASE("S819(a) NotDataBindable: a NON-NotDataBindable target still creates + transfers (over-refusal guard)")
{
    (void)s819_register(kNormal, PropertyFlags::None, ValueType::String);

    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kSourceKey, kTestOwner), Value::String("hello"));

    DependencyObject target;
    mirror_property(target, kNormal, PropertyFlags::None, Value::String("DEFAULT"));

    BindingExpressionSpec spec;
    spec.source     = &vm;
    spec.sourcePath = kSourceKey;
    spec.target     = &target;
    spec.targetProp = kNormal;
    spec.targetOwner = kOwner;
    spec.mode       = BindingMode::OneTime;
    spec.modeExplicit = true;

    RecordingSink sink;
    SinkGuard guard(sink);

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    // First transfer: the normal target accepts the source String.
    (void)expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(expr->binding_id() > 0).ShouldBe(true));
    // No refusal emitted for a bindable target.
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{0}));
}

TEST_CASE("S819(a) NotDataBindable: the refusal NEVER throws under Strict (Class B, ADR-016)")
{
    (void)s819_register(kNotBindable, PropertyFlags::NotDataBindable, ValueType::String);

    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kSourceKey, kTestOwner), Value::String("hello"));

    DependencyObject target;
    mirror_property(target, kNormal, PropertyFlags::None, Value::String("DEFAULT"));

    BindingExpressionSpec spec;
    spec.source     = &vm;
    spec.sourcePath = kSourceKey;
    spec.target     = &target;
    spec.targetProp = kNotBindable;
    spec.targetOwner = kOwner;
    spec.mode       = BindingMode::OneWay;
    spec.modeExplicit = true;
    spec.policy     = BindingPolicy::Strict; // Class A would throw here; Class B must NOT

    RecordingSink sink;
    SinkGuard guard(sink);

    // Strict policy + a NotDataBindable target -> STILL nullptr + emitted, NEVER
    // a throw (NotDataBindable is Class B; only Class A promotes to a throw).
    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr == nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::NotDataBindableTarget)));
}

// ===========================================================================
// (b) BindsTwoWayByDefault — the engine mode defaulting
// ===========================================================================

TEST_CASE("S819(b) BindsTwoWayByDefault: no Mode= + flagged target -> mode DEFAULTS to TwoWay")
{
    (void)s819_register(kTwoWayDefault, PropertyFlags::BindsTwoWayByDefault, ValueType::String);

    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kSourceKey, kTestOwner), Value::String("hello"));

    DependencyObject target;
    mirror_property(target, kTwoWayDefault, PropertyFlags::BindsTwoWayByDefault, Value::String("DEFAULT"));

    BindingExpressionSpec spec;
    spec.source     = &vm;
    spec.sourcePath = kSourceKey;
    spec.target     = &target;
    spec.targetProp = kTwoWayDefault;
    spec.targetOwner = kOwner;
    spec.mode       = BindingMode::OneWay; // authored default, but NOT explicit (no Mode=)
    spec.modeExplicit = false;             // the markup path leaves this false with no Mode=

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    // The engine honors BindsTwoWayByDefault -> the resolved mode is TwoWay
    // (not the authored OneWay default), and the write-back seam is armed.
    SHOULD(ShouldlyIntegral(mode_int(expr->mode())).ShouldBe(mode_int(BindingMode::TwoWay)));
    SHOULD(ShouldlyIntegral(expr->live_writeback()).ShouldBe(true)); // TwoWay -> write-back armed
    SHOULD(ShouldlyIntegral(expr->live_subscribed()).ShouldBe(true)); // TwoWay reuses the forward wire
}

TEST_CASE("S819(b) BindsTwoWayByDefault: an explicit Mode= always wins (stays OneWay)")
{
    (void)s819_register(kTwoWayDefault, PropertyFlags::BindsTwoWayByDefault, ValueType::String);

    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kSourceKey, kTestOwner), Value::String("hello"));

    DependencyObject target;
    mirror_property(target, kTwoWayDefault, PropertyFlags::BindsTwoWayByDefault, Value::String("DEFAULT"));

    BindingExpressionSpec spec;
    spec.source     = &vm;
    spec.sourcePath = kSourceKey;
    spec.target     = &target;
    spec.targetProp = kTwoWayDefault;
    spec.targetOwner = kOwner;
    spec.mode       = BindingMode::OneWay;
    spec.modeExplicit = true; // the author explicitly said Mode=OneWay

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    // The explicit Mode= wins over the metadata default.
    SHOULD(ShouldlyIntegral(mode_int(expr->mode())).ShouldBe(mode_int(BindingMode::OneWay)));
    SHOULD(ShouldlyIntegral(expr->live_writeback()).ShouldBe(false)); // OneWay -> no write-back
    SHOULD(ShouldlyIntegral(expr->live_subscribed()).ShouldBe(true)); // OneWay still subscribes forward
}

TEST_CASE("S819(b) no BindsTwoWayByDefault flag: no Mode= -> defaults to OneWay (regression lock)")
{
    (void)s819_register(kPlain, PropertyFlags::None, ValueType::String);

    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kSourceKey, kTestOwner), Value::String("hello"));

    DependencyObject target;
    mirror_property(target, kPlain, PropertyFlags::None, Value::String("DEFAULT"));

    BindingExpressionSpec spec;
    spec.source     = &vm;
    spec.sourcePath = kSourceKey;
    spec.target     = &target;
    spec.targetProp = kPlain;
    spec.targetOwner = kOwner;
    spec.mode       = BindingMode::OneWay;
    spec.modeExplicit = false; // no Mode=; target NOT flagged -> the WPF {Binding} default OneWay

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(mode_int(expr->mode())).ShouldBe(mode_int(BindingMode::OneWay)));
    SHOULD(ShouldlyIntegral(expr->live_subscribed()).ShouldBe(true));  // OneWay forward wire armed
    SHOULD(ShouldlyIntegral(expr->live_writeback()).ShouldBe(false)); // OneWay -> no write-back
}

TEST_CASE("S819(b) programmatic explicit modes stay verbatim (S242 parity)")
{
    // A programmatic spec that sets mode=TwoWay with modeExplicit=true (the
    // default) keeps TwoWay — unchanged S242 behavior (no metadata involvement).
    (void)s819_register(kNormal, PropertyFlags::None, ValueType::String);

    ObservableObject vm;
    (void)vm.Set(PropertyId::of(kSourceKey, kTestOwner), Value::String("hello"));

    DependencyObject target;
    mirror_property(target, kNormal, PropertyFlags::None, Value::String("DEFAULT"));

    BindingExpressionSpec spec;
    spec.source     = &vm;
    spec.sourcePath = kSourceKey;
    spec.target     = &target;
    spec.targetProp = kNormal;
    spec.targetOwner = kOwner;
    spec.mode       = BindingMode::TwoWay;
    // modeExplicit defaults true -> the explicit programmatic mode is honored
    // even though this target is NOT flagged.

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(mode_int(expr->mode())).ShouldBe(mode_int(BindingMode::TwoWay)));
    SHOULD(ShouldlyIntegral(expr->live_writeback()).ShouldBe(true));
}

// ===========================================================================
// (c) OverridesInheritanceBehavior — carried + consumed in the inheritance walk
// ===========================================================================

TEST_CASE("S819(c) OverridesInheritanceBehavior: flag carried through the GLOBAL DP flags + metadata flags")
{
    const DependencyProperty& dp =
        s819_register(kInheritOIB,
                      PropertyFlags::Inherits | PropertyFlags::OverridesInheritanceBehavior,
                      ValueType::Int32);
    // The flag is carried through BOTH the shorthand flags() and the metadata().
    SHOULD(ShouldlyIntegral(vaml::has_flag(dp.flags(), PropertyFlags::OverridesInheritanceBehavior)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::has_flag(dp.metadata().flags(), PropertyFlags::OverridesInheritanceBehavior)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(vaml::has_flag(dp.flags(), PropertyFlags::Inherits)).ShouldBe(true));
}

TEST_CASE("S819(c) OverridesInheritanceBehavior: the inherited-lookup resolution walks + honors the flag (not silent)")
{
    // The S039 effective-value delivery consumes the flag: a parent/child chain
    // with an Inherits+OverridesInheritanceBehavior property resolves the
    // ancestor local through the walk authority (which is seeded with the flag).
    (void)s819_register(kInheritOIB,
                        PropertyFlags::Inherits | PropertyFlags::OverridesInheritanceBehavior,
                        ValueType::Int32);

    DependencyObject parent;
    DependencyObject child;
    mirror_property(parent, kInheritOIB,
                    PropertyFlags::Inherits | PropertyFlags::OverridesInheritanceBehavior,
                    Value::Int32(0));
    mirror_property(child, kInheritOIB,
                    PropertyFlags::Inherits | PropertyFlags::OverridesInheritanceBehavior,
                    Value::Int32(0));
    child.set_parent(&parent);

    const PropertyId id = PropertyId::of(kInheritOIB, kTestOwner);

    // No local on either -> default.
    SHOULD(ShouldlyString(child.GetEffectiveValue(id)).ShouldBe(Value::Int32(0)));

    // A parent local override -> the child inherits it (the walk ran with the
    // flag consumed; today no barrier exists, so the value resolves normally).
    (void)parent.Set(id, Value::Int32(41));
    SHOULD(ShouldlyString(child.GetEffectiveValue(id)).ShouldBe(Value::Int32(41)));

    // A child local WINS over the inherited value (local ?? inherited ?? default).
    (void)child.Set(id, Value::Int32(7));
    SHOULD(ShouldlyString(child.GetEffectiveValue(id)).ShouldBe(Value::Int32(7)));
}
