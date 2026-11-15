// S242_1 (dep:S242) — BindingPolicy { Strict | Fallback } on BindingExpression.
// Sprint 6.
//
// Proves the S242_1 headline against the REAL vaml::BindingExpression:
//   Strict promotes a Class A path error to a BIND-TIME THROW
//   (BindingPolicyException, emit-then-throw per ADR-016); Class B NEVER throws
//   under either policy (fallback + diagnostic only).
//
// Suite map:
//   1  Spec default policy is Fallback.
//   2  Fallback + malformed path -> create returns nullptr, MalformedPath
//      emitted, no throw (S242 parity).
//   3  Strict + malformed path -> SHOULD_THROW BindingPolicyException with
//      matching kind() + path; the MalformedPath diagnostic is STILL emitted
//      (ADR-016: observation is never the control flow).
//   4  Strict + empty/whitespace path -> a VALID Direct self-binding (no Empty
//      throw — Sprint 11af retired the Empty-as-error premise, S848).
//   5  policy() carried through create (Strict + Fallback round-trip).
//   6  Class B NEVER throws under Strict: MissingProperty / NullIntermediate /
//      NotReadable / converter-fail -> TransferResult + exact reason codes,
//      zero throw (the ADR-016 headline).
//   7  Strict + valid path -> normal Applied transfer works.
//
// S242_1-unique property names (no global/member-name pollution).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow / SHOULD seam

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
using vaml::BindingPolicy;
using vaml::BindingPolicyException;
using vaml::BindingReasonCode;
using vaml::BindingDirection;
using vaml::DependencyObject;
using vaml::IBindingDiagnostics;
using vaml::ObservableObject;
using vaml::PropertyFlags;
using vaml::PropertyId;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::PropertyPathError;
using vaml::TransferResult;
using vaml::Value;
using vaml::ValueType;
using vaml::binding_diagnostics;
using vaml::set_binding_diagnostics;
using vaml::to_binding_direction;

namespace {

// ---- S242_1-unique names ----
const char* kNameText   = "S2421Text";   // source String / target String
const char* kNameNumber = "S2421Number"; // source Int32 / target Int32
const char* kNameEmpty  = "S2421Empty";  // source (null-valued)
const char* kNameNope   = "S2421Nope";   // absent source name (MissingProperty)

PropertyKey key(const std::string& name, ValueType t)
{
    return PropertyKey(name, t, "S2421Test", PropertyFlags::None);
}

DependencyObject make_target()
{
    DependencyObject obj;
    obj.register_property(key(kNameText, ValueType::String),
        PropertyMetadata::WithDefault(Value::String("DEFAULT_TEXT"), PropertyFlags::None));
    obj.register_property(key(kNameNumber, ValueType::Int32),
        PropertyMetadata::WithDefault(Value::Int32(-1), PropertyFlags::None));
    return obj;
}

// RAII guard: swaps in the recorder, restores the PERMANENT default sink at
// scope exit (the process-global sink must be restored so tests can run in any
// order). Restoring to default_binding_diagnostics() — never a captured
// `previous` reference — guarantees the restored sink outlives every test (a
// captured reference can dangle into a prior test's freed stack RecordingSink,
// which is the exact use-after-free that surfaces as a stray `reasons[0]`
// crash in a full in-process run).
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
int tr_int(TransferResult r) { return static_cast<int>(r); }
int err_int(PropertyPathError e) { return static_cast<int>(e); }

} // namespace

// ===========================================================================
// 1 - DEFAULT POLICY
// ===========================================================================

TEST_CASE("S242_1 policy: spec default is Fallback")
{
    BindingExpressionSpec spec;
    SHOULD(ShouldlyIntegral(static_cast<int>(spec.policy)).ShouldBe(static_cast<int>(BindingPolicy::Fallback)));
}

// ===========================================================================
// 2 - FALLBACK (default) PARITY with S242
// ===========================================================================

TEST_CASE("S242_1 Fallback: malformed path -> create nullptr, MalformedPath emitted, no throw")
{
    ObservableObject vm;
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = "S2421Text["; // UnterminatedIndexer — a genuine Class A error
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.targetOwner = "S2421Test";
    spec.mode = BindingMode::OneTime;
    // policy left at default (Fallback)

    auto expr = BindingExpression::create(spec);
    SHOULD(ShouldlyIntegral(expr == nullptr).ShouldBe(true));
    // Target untouched — nothing bound, nothing applied.
    SHOULD(ShouldlyString(target.Get(PropertyId::of("S2421Text", "S2421Test"))).ShouldBe(Value::String("DEFAULT_TEXT")));
    // The diagnostic WAS emitted (observation is orthogonal to the policy).
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::MalformedPath)));
}

// ===========================================================================
// 3 - STRICT + MALFORMED PATH -> BIND-TIME THROW
// ===========================================================================

TEST_CASE("S242_1 Strict: malformed path -> throws BindingPolicyException; diagnostic still emitted")
{
    ObservableObject vm;
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = "S2421Text["; // UnterminatedIndexer — a genuine Class A error
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.targetOwner = "S2421Test";
    spec.mode = BindingMode::OneTime;
    spec.policy = BindingPolicy::Strict;

    // Strict PROMOTES the Class A error to a bind-time throw.
    bool threw = false;
    PropertyPathError caughtKind = PropertyPathError::Empty;
    std::string caughtPath;
    try
    {
        (void)BindingExpression::create(spec);
    }
    catch (const BindingPolicyException& e)
    {
        threw = true;
        caughtKind = e.kind();
        caughtPath = e.path();
    }
    SHOULD(ShouldlyIntegral(threw).ShouldBe(true));
    // The exception names the authored path.
    SHOULD(ShouldlyString(caughtPath).ShouldBe(std::string("S2421Text[")));
    // A Class A syntax kind (not a degenerate Empty).
    SHOULD(ShouldlyIntegral(err_int(caughtKind)).ShouldNotBe(err_int(PropertyPathError::Empty)));

    // ADR-016: the diagnostic is OBSERVATION — it is emitted alongside the throw,
    // it is never the control flow (the throw is).
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::MalformedPath)));
}

// ===========================================================================
// 4 - STRICT + EMPTY/WHITESPACE PATH -> EMPTY-KINDED THROW
// ===========================================================================

TEST_CASE("S242_1 Strict: empty/whitespace path -> a valid Direct self-binding (no Empty throw)")
{
    // Sprint 11af (S845/S847): an empty / whitespace-only path is a VALID
    // Direct self-binding (WPF PathParser.cs:83-89) — it is NOT a Class A
    // Empty error, so Strict does NOT throw (a valid path is never a Strict
    // bind-time throw). The retired Empty-path-kinded throw is gone (S848).
    ObservableObject vm;
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = "   "; // whitespace-only -> Direct (valid, not an error)
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.targetOwner = "S2421Test";
    spec.mode = BindingMode::OneTime;
    spec.policy = BindingPolicy::Strict;

    // Strict with a VALID (Direct) path is NOT a throw — create succeeds.
    bool threw = false;
    try
    {
        (void)BindingExpression::create(spec);
    }
    catch (const BindingPolicyException&)
    {
        threw = true;
    }
    SHOULD(ShouldlyIntegral(threw).ShouldBe(false));
    // No Empty diagnostic for a valid Direct path.
    SHOULD(ShouldlyIntegral(sink.reasons.empty()).ShouldBe(true));
}

// ===========================================================================
// 5 - POLICY CARRIED THROUGH create
// ===========================================================================

TEST_CASE("S242_1 policy: policy() is carried through create (Strict and Fallback)")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of("S2421Text", "ObservableObject"), Value::String("x"));
    DependencyObject target = make_target();

    BindingExpressionSpec strictSpec;
    strictSpec.source = &vm;
    strictSpec.sourcePath = kNameText;
    strictSpec.target = &target;
    strictSpec.targetProp = kNameText;
    strictSpec.mode = BindingMode::OneTime;
    strictSpec.policy = BindingPolicy::Strict;
    auto e1 = BindingExpression::create(strictSpec);
    SHOULD(ShouldlyIntegral(e1 != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(e1->policy())).ShouldBe(static_cast<int>(BindingPolicy::Strict)));

    BindingExpressionSpec fallbackSpec = strictSpec;
    fallbackSpec.policy = BindingPolicy::Fallback;
    auto e2 = BindingExpression::create(fallbackSpec);
    SHOULD(ShouldlyIntegral(e2 != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(e2->policy())).ShouldBe(static_cast<int>(BindingPolicy::Fallback)));
}

// ===========================================================================
// 6 - CLASS B NEVER THROWS UNDER STRICT (ADR-016 headline)
// ===========================================================================

TEST_CASE("S242_1 Strict: Class B (MissingProperty) never throws -> ResolveFailed + reason, no fallback")
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
    spec.targetOwner = "S2421Test";
    spec.mode = BindingMode::OneTime;
    spec.policy = BindingPolicy::Strict;

    auto expr = BindingExpression::create(spec); // valid path, no throw
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    // Strict must NOT promote Class B to a throw.
    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::ResolveFailed)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of("S2421Text", "S2421Test"))).ShouldBe(Value::String("DEFAULT_TEXT")));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::MissingProperty)));
}

TEST_CASE("S242_1 Strict: Class B (NullIntermediate) never throws -> FallbackApplied + NullIntermediate reason")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of("S2421Empty", "ObservableObject"), Value::Null());
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = std::string(kNameEmpty) + "." + kNameText; // intermediate null
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.targetOwner = "S2421Test";
    spec.mode = BindingMode::OneTime;
    spec.policy = BindingPolicy::Strict;
    spec.fallback = Value::String("FB");

    auto expr = BindingExpression::create(spec); // valid path, no throw
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::FallbackApplied)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of("S2421Text", "S2421Test"))).ShouldBe(Value::String("FB")));
    // NullIntermediate + then FallbackApplied (zero throw).
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{2}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::NullIntermediate)));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[1])).ShouldBe(rc_int(BindingReasonCode::FallbackApplied)));
}

TEST_CASE("S242_1 Strict: Class B (NotReadable) never throws -> ResolveFailed + NotReadable reason")
{
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = nullptr; // -> not-readable (Class B runtime outcome)
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.targetOwner = "S2421Test";
    spec.mode = BindingMode::OneTime;
    spec.policy = BindingPolicy::Strict;

    auto expr = BindingExpression::create(spec); // valid path, no throw
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::ResolveFailed)));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::NotReadable)));
}

TEST_CASE("S242_1 Strict: Class B (converter fail) never throws -> ConverterFailed + fallback applied")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of("S2421Text", "ObservableObject"), Value::String("x"));
    DependencyObject target = make_target();
    RecordingSink sink;
    SinkGuard guard(sink);

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameNumber;
    spec.targetOwner = "S2421Test";
    spec.mode = BindingMode::OneTime;
    spec.policy = BindingPolicy::Strict;
    spec.converter.convert = [](const Value&) -> BindingConversion {
        throw std::runtime_error("boom"); // a throwing converter is Class B, recovered
    };
    spec.converterName = "S2421Throws";
    spec.fallback = Value::Int32(5);

    auto expr = BindingExpression::create(spec); // valid path, no throw
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    // Strict does NOT promote a converter failure to a throw — fallback applies.
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::FallbackApplied)));
    SHOULD(ShouldlyIntegral(target.Get(PropertyId::of("S2421Number", "S2421Test")).get<std::int32_t>()).ShouldBe(5));
    SHOULD(ShouldlyIntegral(sink.reasons.size()).ShouldBe(std::size_t{2}));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[0])).ShouldBe(rc_int(BindingReasonCode::ConverterFailed)));
    SHOULD(ShouldlyIntegral(rc_int(sink.reasons[1])).ShouldBe(rc_int(BindingReasonCode::FallbackApplied)));
}

// ===========================================================================
// 7 - STRICT + VALID PATH -> NORMAL TRANSFER
// ===========================================================================

TEST_CASE("S242_1 Strict: valid path -> normal Applied transfer works under Strict")
{
    ObservableObject vm;
    (void)vm.Set(PropertyId::of("S2421Text", "ObservableObject"), Value::String("Hello"));
    DependencyObject target = make_target();

    BindingExpressionSpec spec;
    spec.source = &vm;
    spec.sourcePath = kNameText;
    spec.target = &target;
    spec.targetProp = kNameText;
    spec.targetOwner = "S2421Test";
    spec.mode = BindingMode::OneTime;
    spec.policy = BindingPolicy::Strict;

    auto expr = BindingExpression::create(spec); // no throw — the path is valid
    SHOULD(ShouldlyIntegral(expr != nullptr).ShouldBe(true));

    TransferResult r = expr->TransferSourceToTarget();
    SHOULD(ShouldlyIntegral(tr_int(r)).ShouldBe(tr_int(TransferResult::Applied)));
    SHOULD(ShouldlyString(target.Get(PropertyId::of("S2421Text", "S2421Test"))).ShouldBe(Value::String("Hello")));
}
