// S240_1 — BindingDiagnostics record + sink (dep:S240). Sprint 6.
//
// Class A/B tagging authority (binding_class_of, exhaustive over all 11 reason
// codes), the two factories (diagnostic_from_property_read maps MissingProperty/
// NullIntermediate/NotReadable -> Class B; the degenerate empty-path -> EmptyPath
// Class A; diagnostic_from_path_error maps a PropertyPathError -> MalformedPath /
// EmptyPath Class A), the NO-OP default sink (emit is a safe no-op), and the
// opt-in/recurrent set_binding_diagnostics (a recording sink receives emission
// through binding_diagnostics().emit; set returns the previous sink; restore
// default -> no-op again).
//
// Pure value-domain: registers NO property names, no DPs, no window/GPU.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam)

#include "vaml/core/BindingDiagnostics.h"
#include "vaml/core/PropertyPath.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using vaml::BindingClass;
using vaml::BindingDiagnostic;
using vaml::BindingDirection;
using vaml::BindingReasonCode;
using vaml::IBindingDiagnostics;
using vaml::NullBindingDiagnostics;
using vaml::PropertyPath;
using vaml::PropertyPathError;
using vaml::PropertyRead;
using vaml::binding_class_of;
using vaml::binding_diagnostics;
using vaml::default_binding_diagnostics;
using vaml::diagnostic_from_path_error;
using vaml::diagnostic_from_property_read;
using vaml::set_binding_diagnostics;

namespace {

int rc_int(BindingReasonCode c) { return static_cast<int>(c); }
int cls_int(BindingClass c) { return static_cast<int>(c); }
int dir_int(BindingDirection d) { return static_cast<int>(d); }

// A recording sink proving the emit path end-to-end (app opt-in).
struct RecordingSink final : IBindingDiagnostics
{
    std::vector<BindingDiagnostic> seen;
    void emit(const BindingDiagnostic& d) override { seen.push_back(d); }
};

} // namespace

// ===========================================================================
// CLASS A/B TAGGING AUTHORITY (exhaustive)
// ===========================================================================

TEST_CASE("S240_1 tagging: all three Class A arms map to StaticAuthoring")
{
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::EmptyPath))).ShouldBe(cls_int(BindingClass::StaticAuthoring)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::MalformedPath))).ShouldBe(cls_int(BindingClass::StaticAuthoring)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::ConverterSignatureMismatch))).ShouldBe(cls_int(BindingClass::StaticAuthoring)));
}

TEST_CASE("S240_1 tagging: all eight Class B arms map to DefinedRuntimeOutcome")
{
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::NullIntermediate))).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::MissingProperty))).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::NotReadable))).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::ConverterFailed))).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::FallbackApplied))).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::SetTargetFailed))).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::WriteBackRejected))).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
    SHOULD(ShouldlyIntegral(cls_int(binding_class_of(BindingReasonCode::Detached))).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
}

// ===========================================================================
// FACTORY: diagnostic_from_property_read (S240 resolve Class B outcomes)
// ===========================================================================

TEST_CASE("S240_1 from_property_read: MissingProperty -> Class B MissingProperty, copies context")
{
    PropertyPath p = PropertyPath::parse("Nope.S240Name");
    PropertyRead r{false, PropertyPathError::MissingProperty, vaml::Value::Null(), 0};

    BindingDiagnostic d = diagnostic_from_property_read(
        42, "S240Target", BindingDirection::OneWay,
        "S240SourceType", "S240TargetType", "S240Conv",
        p, r, "expected failure", "xaml/app.xaml", 7);

    SHOULD(ShouldlyIntegral(d.bindingId).ShouldBe(std::uint64_t{42}));
    SHOULD(ShouldlyString(d.sourcePath).ShouldBe("Nope.S240Name"));
    SHOULD(ShouldlyString(d.targetProp).ShouldBe("S240Target"));
    SHOULD(ShouldlyIntegral(dir_int(d.direction)).ShouldBe(dir_int(BindingDirection::OneWay)));
    SHOULD(ShouldlyIntegral(rc_int(d.reasonCode)).ShouldBe(rc_int(BindingReasonCode::MissingProperty)));
    SHOULD(ShouldlyString(d.sourceType).ShouldBe("S240SourceType"));
    SHOULD(ShouldlyString(d.targetType).ShouldBe("S240TargetType"));
    SHOULD(ShouldlyString(d.converter).ShouldBe("S240Conv"));
    SHOULD(ShouldlyString(d.detail).ShouldBe("expected failure"));
    SHOULD(ShouldlyString(d.sourceFile).ShouldBe("xaml/app.xaml"));
    SHOULD(ShouldlyIntegral(d.sourceLine).ShouldBe(7u));
    SHOULD(ShouldlyIntegral(cls_int(d.bindingClass())).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
    // timestamp is stamped now (non-default time_point).
    SHOULD(ShouldlyIntegral(d.timestamp != std::chrono::system_clock::time_point{}).ShouldBe(true));
}

TEST_CASE("S240_1 from_property_read: NullIntermediate -> Class B NullIntermediate")
{
    PropertyPath p = PropertyPath::parse("S240Empty.S240Name");
    PropertyRead r{false, PropertyPathError::NullIntermediate, vaml::Value::Null(), 1};

    BindingDiagnostic d = diagnostic_from_property_read(1, "T", BindingDirection::OneTime, "", "", "", p, r);
    SHOULD(ShouldlyIntegral(rc_int(d.reasonCode)).ShouldBe(rc_int(BindingReasonCode::NullIntermediate)));
    SHOULD(ShouldlyIntegral(cls_int(d.bindingClass())).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
}

TEST_CASE("S240_1 from_property_read: NotReadable -> Class B NotReadable")
{
    PropertyPath p = PropertyPath::parse("[0]");
    PropertyRead r{false, PropertyPathError::NotReadable, vaml::Value::Null(), 0};

    BindingDiagnostic d = diagnostic_from_property_read(1, "T", BindingDirection::OneTime, "", "", "", p, r);
    SHOULD(ShouldlyIntegral(rc_int(d.reasonCode)).ShouldBe(rc_int(BindingReasonCode::NotReadable)));
    SHOULD(ShouldlyIntegral(cls_int(d.bindingClass())).ShouldBe(cls_int(BindingClass::DefinedRuntimeOutcome)));
}

TEST_CASE("S240_1 from_property_read: degenerate empty path -> EmptyPath Class A")
{
    // An invalid/empty path handed to the evaluator yields read.ok==false with
    // error==Empty. The factory maps this to the CLass A EmptyPath reason.
    PropertyPath p;
    PropertyPathError err;
    (void)PropertyPath::try_parse("", p, &err);
    PropertyRead r{false, PropertyPathError::Empty, vaml::Value::Null(), 0};

    BindingDiagnostic d = diagnostic_from_property_read(1, "T", BindingDirection::OneTime, "", "", "", p, r);
    SHOULD(ShouldlyIntegral(rc_int(d.reasonCode)).ShouldBe(rc_int(BindingReasonCode::EmptyPath)));
    SHOULD(ShouldlyIntegral(cls_int(d.bindingClass())).ShouldBe(cls_int(BindingClass::StaticAuthoring)));
}

// ===========================================================================
// FACTORY: diagnostic_from_path_error (Class A parse failures)
// ===========================================================================

TEST_CASE("S240_1 from_path_error: a syntax error -> MalformedPath Class A")
{
    // "Items[0" is an UnterminatedIndexer Class A parse error.
    BindingDiagnostic d = diagnostic_from_path_error(
        9, "Items[0", "S240Target", BindingDirection::OneWay,
        "", "", PropertyPathError::UnterminatedIndexer, "bad path", "", 0);

    SHOULD(ShouldlyIntegral(rc_int(d.reasonCode)).ShouldBe(rc_int(BindingReasonCode::MalformedPath)));
    SHOULD(ShouldlyIntegral(cls_int(d.bindingClass())).ShouldBe(cls_int(BindingClass::StaticAuthoring)));
    SHOULD(ShouldlyString(d.sourcePath).ShouldBe("Items[0"));
    SHOULD(ShouldlyString(d.targetProp).ShouldBe("S240Target"));
    SHOULD(ShouldlyString(d.detail).ShouldBe("bad path"));
}

TEST_CASE("S240_1 from_path_error: the Empty kind maps to EmptyPath Class A")
{
    BindingDiagnostic d = diagnostic_from_path_error(1, "", "T", BindingDirection::OneTime, "", "",
                                                     PropertyPathError::Empty, "", "", 0);
    SHOULD(ShouldlyIntegral(rc_int(d.reasonCode)).ShouldBe(rc_int(BindingReasonCode::EmptyPath)));
    SHOULD(ShouldlyIntegral(cls_int(d.bindingClass())).ShouldBe(cls_int(BindingClass::StaticAuthoring)));
}

// ===========================================================================
// SINK: NO-OP default + opt-in/restore
// ===========================================================================

TEST_CASE("S240_1 sink: the default sink is a safe no-op emit")
{
    IBindingDiagnostics& noop = default_binding_diagnostics();
    // Emitting onto the no-op is a no-op; it must not throw.
    BindingDiagnostic d;
    d.bindingId = 1;
    d.reasonCode = BindingReasonCode::MissingProperty;
    noop.emit(d);
    SHOULD(ShouldlyIntegral(true).ShouldBe(true)); // reached past a no-op emit
}

TEST_CASE("S240_1 sink: an opted-in recording sink receives emission; set returns the previous sink")
{
    // Note: the current sink is process-global. Capture the previous so we can
    // restore it (tests may run in any order) — restore at the end.
    IBindingDiagnostics& original = binding_diagnostics();
    RecordingSink recorder;
    IBindingDiagnostics& returned = set_binding_diagnostics(recorder);
    SHOULD(ShouldlyIntegral(&returned == &original).ShouldBe(true));
    // set returns the sink that was current BEFORE the swap.
    SHOULD(ShouldlyIntegral(&returned == &original).ShouldBe(true));

    // Emit through the free sink accessor -> the recorder captures it.
    BindingDiagnostic d;
    d.bindingId = 123;
    d.sourcePath = "Nope";
    d.reasonCode = BindingReasonCode::MissingProperty;
    binding_diagnostics().emit(d);

    SHOULD(ShouldlyIntegral(recorder.seen.size()).ShouldBe(std::size_t{1}));
    SHOULD(ShouldlyIntegral(recorder.seen[0].bindingId).ShouldBe(std::uint64_t{123}));
    SHOULD(ShouldlyIntegral(rc_int(recorder.seen[0].reasonCode)).ShouldBe(rc_int(BindingReasonCode::MissingProperty)));

    // Restore the default -> subsequent emits are no-ops again.
    IBindingDiagnostics& back = set_binding_diagnostics(original);
    SHOULD(ShouldlyIntegral(&back == &recorder).ShouldBe(true));
    SHOULD(ShouldlyIntegral(&binding_diagnostics() == &original).ShouldBe(true));

    binding_diagnostics().emit(BindingDiagnostic{}); // no-op, must not throw
    SHOULD(ShouldlyIntegral(recorder.seen.size()).ShouldBe(std::size_t{1})); // unchanged
}
